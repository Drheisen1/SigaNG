#include "SIGA/AnimationHandler.h"
#include "SIGA/SlowMotion.h"
#include "SIGA/Config.h"

namespace SIGA {

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
        auto slowMgr = SlowMotionManager::GetSingleton();

        // Log only important events
        logger::trace("Animation event: '{}' from {}", eventName, isPlayer ? "Player" : actor->GetName());

        // BOW EVENTS
        if (eventName == "BowDrawn") {
            logger::debug("BowDrawn event");
            OnBowDrawn(actor);
        }
        else if (eventName == "bowRelease") {
            logger::debug("Bow release event");
            slowMgr->RemoveSlowdown(actor, SlowType::Bow);
            slowMgr->RemoveSlowdown(actor, SlowType::Crossbow);
        }

        // CASTING EVENTS
        else if (eventName == "BeginCastLeft") {
            logger::debug("BeginCastLeft event");
            OnBeginCastLeft(actor);
        }
        else if (eventName == "BeginCastRight") {
            logger::debug("BeginCastRight event");
            OnBeginCastRight(actor);
        }

        // Add CastStop event
        else if (eventName == "CastStop") {
            logger::debug("CastStop event");
            OnCastRelease(actor);
        }

        // Additional cast stop events
        else if (eventName == "CastOKStop" || eventName == "InterruptCast") {
            // Only process if actor is actually slowed
            if (slowMgr->IsActorSlowed(actor)) {
                logger::debug("Cast interrupted: {}", eventName);
                OnCastRelease(actor);
            }
        }

        // GENERIC CLEANUP 
        else if (eventName == "attackStop") {
            // Only clear slowdowns if we're actually slowed
            if (slowMgr->IsActorSlowed(actor)) {
                logger::debug("attackStop while slowed - safety cleanup");
                OnAttackStop(actor);
            }
        }

        // WEAPON DRAW/SHEATHE EVENTS
        else if (eventName == "WeaponSheathe" || eventName == "weaponSheathe") {

            // Clear slowdowns
            if (slowMgr->IsActorSlowed(actor)) {
                logger::debug("Weapon state changed - clearing slowdowns");
                slowMgr->ClearAllSlowdowns(actor);
            }
        }


        static int cleanupCounter = 0;
        if (++cleanupCounter >= 100) {
            SlowMotionManager::GetSingleton()->CleanupInactiveStates();
            cleanupCounter = 0;
        }

        return RE::BSEventNotifyControl::kContinue;
    }

    void AnimationEventHandler::OnBowDrawn(RE::Actor* actor) {
        auto config = Config::GetSingleton();

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

        auto leftSpell = actor->GetActorRuntimeData().selectedSpells[RE::Actor::SlotTypes::kLeftHand];
        if (!leftSpell) {
            logger::debug("No spell in left hand");
            return;
        }

        // Check if spell modifies SpeedMult - if so, skip slowdown
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

        auto rightSpell = actor->GetActorRuntimeData().selectedSpells[RE::Actor::SlotTypes::kRightHand];
        if (!rightSpell) {
            logger::debug("No spell in right hand");
            return;
        }

        // Check if spell modifies SpeedMult - if so, skip slowdown
        if (SpellModifiesSpeed(rightSpell)) {
            logger::debug("Right spell modifies speed - skipping slowdown");
            return;
        }

        float skillLevel = GetMagicSkillLevel(actor, rightSpell);
        logger::debug("Right hand: {} (skill: {})", rightSpell->GetName(), skillLevel);
        SlowMotionManager::GetSingleton()->ApplySlowdown(actor, SlowType::CastRight, skillLevel);
    }

    void AnimationEventHandler::OnCastRelease(RE::Actor* actor) {
        // remove all casting slowdowns when cast stops
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

        // try to cast to SpellItem
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

        // Check all magic effects in the spell
        for (auto effect : spellItem->effects) {
            if (effect && effect->baseEffect) {
                // Check if effect modifies SpeedMult actor value
                if (effect->baseEffect->data.primaryAV == RE::ActorValue::kSpeedMult) {
                    return true;
                }
            }
        }

        return false;
    }

} 