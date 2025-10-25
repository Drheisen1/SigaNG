#include "SIGA/AnimationHandler.h"
#include "SIGA/SlowMotion.h"
#include "SIGA/Config.h"
#include <unordered_map>

namespace SIGA {

    // OPTIMIZATION: Event type enum for fast switch instead of string comparisons
    enum class AnimEventType {
        Unknown,
        BowDrawn,
        BowRelease,
        BeginCastLeft,
        BeginCastRight,
        CastStop,
        CastOKStop,
        InterruptCast,
        AttackStop,
        WeaponSheathe,
    };

    // OPTIMIZATION: Hash map for O(1) event lookup instead of O(n) string comparisons
    static const std::unordered_map<std::string_view, AnimEventType> EVENT_LOOKUP = {
        {"BowDrawn", AnimEventType::BowDrawn},
        {"bowRelease", AnimEventType::BowRelease},
        {"BeginCastLeft", AnimEventType::BeginCastLeft},
        {"BeginCastRight", AnimEventType::BeginCastRight},
        {"CastStop", AnimEventType::CastStop},
        {"CastOKStop", AnimEventType::CastOKStop},
        {"InterruptCast", AnimEventType::InterruptCast},
        {"attackStop", AnimEventType::AttackStop},
        {"WeaponSheathe", AnimEventType::WeaponSheathe},
        {"weaponSheathe", AnimEventType::WeaponSheathe},
    };

    AnimationEventHandler* AnimationEventHandler::GetSingleton() {
        static AnimationEventHandler singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl AnimationEventHandler::ProcessEvent(
        const RE::BSAnimationGraphEvent* a_event,
        RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource)
    {
        if (!a_event || !a_event->holder) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto actor = const_cast<RE::Actor*>(a_event->holder->As<RE::Actor>());
        if (!actor) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Handle player
        bool isPlayer = actor->IsPlayerRef();

        // Handle NPCs
        if (!isPlayer) {
            auto config = Config::GetSingleton();

            // Check if NPC support is enabled
            if (!config->applyToNPCs) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Check if NPC is in combat
            if (!actor->IsInCombat()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // NPC passed all checks, process the event
            logger::trace("Processing NPC event: {}", actor->GetName());
        }

        std::string_view eventName = a_event->tag;

        // OPTIMIZATION: Single hash lookup instead of multiple string comparisons
        auto eventIt = EVENT_LOOKUP.find(eventName);
        if (eventIt == EVENT_LOOKUP.end()) {
            // Unknown event, ignore
            return RE::BSEventNotifyControl::kContinue;
        }

        logger::trace("Animation event: '{}' from {}", eventName, isPlayer ? "Player" : actor->GetName());

        auto slowMgr = SlowMotionManager::GetSingleton();

        // OPTIMIZATION: Switch on enum instead of string comparisons
        switch (eventIt->second) {
        case AnimEventType::BowDrawn:
            logger::debug("Bow drawn event");
            OnBowDrawn(actor);
            break;

        case AnimEventType::BowRelease:
            logger::debug("Bow release event");
            slowMgr->RemoveSlowdown(actor, SlowType::Bow);
            slowMgr->RemoveSlowdown(actor, SlowType::Crossbow);
            break;

        case AnimEventType::BeginCastLeft:
            logger::debug("BeginCastLeft event");
            OnBeginCastLeft(actor);
            break;

        case AnimEventType::BeginCastRight:
            logger::debug("BeginCastRight event");
            OnBeginCastRight(actor);
            break;

        case AnimEventType::CastStop:
            logger::debug("CastStop event");
            OnCastRelease(actor);
            break;

        case AnimEventType::CastOKStop:
        case AnimEventType::InterruptCast:
            if (slowMgr->IsActorSlowed(actor)) {
                logger::debug("Cast interrupted: {}", eventName);
                OnCastRelease(actor);
            }
            break;

        case AnimEventType::AttackStop:
            if (slowMgr->IsActorSlowed(actor)) {
                logger::debug("attackStop while slowed - clearing slowdowns");
                OnAttackStop(actor);
            }
            break;

        case AnimEventType::WeaponSheathe:
            if (slowMgr->IsActorSlowed(actor)) {
                logger::debug("Weapon state changed - clearing slowdowns");
                slowMgr->ClearAllSlowdowns(actor);
            }
            break;

        default:
            break;
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    void AnimationEventHandler::OnBowDrawn(RE::Actor* actor) {
        auto config = Config::GetSingleton();

        bool isPlayer = actor->IsPlayerRef();

        // Check if slowdown should apply based on actor type
        if (config->applySlowdownCastingToNPCsOnly) {
            // NPCs only mode - skip player
            if (isPlayer) {
                logger::trace("Bow slowdown skipped for player (NPCs only mode)");
                return;
            }
        }
        else {
            // Normal mode - NPCs need applyToNPCs enabled
            if (!isPlayer && !config->applyToNPCs) {
                logger::trace("Bow slowdown disabled for NPCs");
                return;
            }
        }

        float archerySkill = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kArchery);

        // Determine weapon type
        auto equippedObject = actor->GetEquippedObject(false);
        bool isCrossbow = false;

        if (equippedObject) {
            auto weapon = equippedObject->As<RE::TESObjectWEAP>();
            if (weapon) {
                isCrossbow = (weapon->GetWeaponType() == RE::WEAPON_TYPE::kCrossbow);
            }
        }

        SlowType type = isCrossbow ? SlowType::Crossbow : SlowType::Bow;

        // Check if this type is enabled
        if (isCrossbow && !config->enableCrossbowDebuff) {
            logger::debug("Crossbow debuff disabled in config");
            return;
        }
        if (!isCrossbow && !config->enableBowDebuff) {
            logger::debug("Bow debuff disabled in config");
            return;
        }

        logger::debug("Applying {} slowdown (skill: {})", isCrossbow ? "crossbow" : "bow", archerySkill);
        SlowMotionManager::GetSingleton()->ApplySlowdown(actor, type, archerySkill);
    }

    void AnimationEventHandler::OnBeginCastLeft(RE::Actor* actor) {
        auto config = Config::GetSingleton();
        if (!config->enableCastDebuff) {
            return;
        }

        bool isPlayer = actor->IsPlayerRef();

        // Check if casting slowdown should apply based on actor type
        if (config->applySlowdownCastingToNPCsOnly) {
            // NPCs only mode - skip player
            if (isPlayer) {
                logger::trace("Casting slowdown skipped for player (NPCs only mode)");
                return;
            }
        }
        else {
            // Normal mode - NPCs need applyToNPCs enabled
            if (!isPlayer && !config->applyToNPCs) {
                logger::trace("Casting slowdown disabled for NPCs");
                return;
            }
        }

        auto leftSpell = actor->GetActorRuntimeData().selectedSpells[RE::Actor::SlotTypes::kLeftHand];
        if (!leftSpell) {
            logger::debug("No spell in left hand");
            return;
        }

        if (SpellModifiesSpeed(leftSpell)) {
            logger::debug("Left spell modifies speed - skipping slowdown");
            return;
        }

        float skillLevel = GetMagicSkillLevel(actor, leftSpell);
        logger::debug("Left hand: {} (skill: {})", leftSpell->GetName(), skillLevel);
        SlowMotionManager::GetSingleton()->ApplySlowdown(actor, SlowType::CastLeft, skillLevel);
    }

    void AnimationEventHandler::OnBeginCastRight(RE::Actor* actor) {
        auto config = Config::GetSingleton();
        if (!config->enableCastDebuff) {
            return;
        }

        bool isPlayer = actor->IsPlayerRef();

        // Check if casting slowdown should apply based on actor type
        if (config->applySlowdownCastingToNPCsOnly) {
            // NPCs only mode - skip player
            if (isPlayer) {
                logger::trace("Casting slowdown skipped for player (NPCs only mode)");
                return;
            }
        }
        else {
            // Normal mode - NPCs need applyToNPCs enabled
            if (!isPlayer && !config->applyToNPCs) {
                logger::trace("Casting slowdown disabled for NPCs");
                return;
            }
        }

        auto rightSpell = actor->GetActorRuntimeData().selectedSpells[RE::Actor::SlotTypes::kRightHand];
        if (!rightSpell) {
            logger::debug("No spell in right hand");
            return;
        }

        if (SpellModifiesSpeed(rightSpell)) {
            logger::debug("Right spell modifies speed - skipping slowdown");
            return;
        }

        float skillLevel = GetMagicSkillLevel(actor, rightSpell);
        logger::debug("Right hand: {} (skill: {})", rightSpell->GetName(), skillLevel);
        SlowMotionManager::GetSingleton()->ApplySlowdown(actor, SlowType::CastRight, skillLevel);
    }

    void AnimationEventHandler::OnCastRelease(RE::Actor* actor) {
        auto slowMgr = SlowMotionManager::GetSingleton();
        slowMgr->RemoveSlowdown(actor, SlowType::CastLeft);
        slowMgr->RemoveSlowdown(actor, SlowType::CastRight);
        slowMgr->RemoveSlowdown(actor, SlowType::DualCast);
        logger::debug("Cast released, removed all casting slowdowns");
    }

    void AnimationEventHandler::OnAttackStop(RE::Actor* actor) {
        SlowMotionManager::GetSingleton()->ClearAllSlowdowns(actor);
    }

    float AnimationEventHandler::GetMagicSkillLevel(RE::Actor* actor, RE::MagicItem* spell) {
        if (!spell) return 0.0f;

        auto spellItem = spell->As<RE::SpellItem>();
        if (!spellItem) {
            logger::warn("Could not cast spell to SpellItem");
            return 0.0f;
        }

        auto avOwner = actor->AsActorValueOwner();
        auto school = spellItem->GetAssociatedSkill();

        if (school == RE::ActorValue::kNone) {
            // Average all magic schools
            float total = 0.0f;
            total += avOwner->GetActorValue(RE::ActorValue::kDestruction);
            total += avOwner->GetActorValue(RE::ActorValue::kRestoration);
            total += avOwner->GetActorValue(RE::ActorValue::kAlteration);
            total += avOwner->GetActorValue(RE::ActorValue::kConjuration);
            total += avOwner->GetActorValue(RE::ActorValue::kIllusion);
            return total * 0.2f;
        }

        return avOwner->GetActorValue(school);
    }

    bool AnimationEventHandler::SpellModifiesSpeed(RE::MagicItem* spell) {
        if (!spell) return false;

        auto spellItem = spell->As<RE::SpellItem>();
        if (!spellItem) return false;

        for (auto effect : spellItem->effects) {
            if (effect && effect->baseEffect) {
                if (effect->baseEffect->data.primaryAV == RE::ActorValue::kSpeedMult) {
                    return true;
                }
            }
        }

        return false;
    }

}