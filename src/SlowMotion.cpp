#include "SIGA/SlowMotion.h"
#include "SIGA/Config.h"

namespace SIGA {

    SlowMotionManager* SlowMotionManager::GetSingleton() {
        static SlowMotionManager singleton;
        return &singleton;
    }

    void SlowMotionManager::ApplySlowdown(RE::Actor* actor, SlowType type, float skillLevel) {
        if (!actor) {
            logger::warn("ApplySlowdown called with null actor");
            return;
        }

        auto formID = actor->GetFormID();
        auto& state = actorStates[formID];

        if (!IsActorSlowed(actor)) {
            float currentSpeed = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeedMult);
            state.baseSpeedDelta = currentSpeed - 100.0f;
            logger::debug("Captured speed delta: {} (current speed: {})", state.baseSpeedDelta, currentSpeed);
        }

        logger::debug("ApplySlowdown: type={}, skillLevel={}", static_cast<int>(type), skillLevel);

        switch (type) {
        case SlowType::Bow:
        case SlowType::Crossbow:
            state.bowSlowActive = true;
            break;
        case SlowType::CastLeft:
            state.castLeftActive = true;
            break;
        case SlowType::CastRight:
            state.castRightActive = true;
            break;
        }

        SlowType typeToUse = type;
        if (state.castLeftActive && state.castRightActive) {
            state.dualCastActive = true;
            typeToUse = SlowType::DualCast;
            logger::debug("Dual casting detected!");
        }

        float multiplier = CalculateSpeedMultiplier(skillLevel, typeToUse);
        float targetSpeed = (100.0f * multiplier) + state.baseSpeedDelta;
        float currentSpeed = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeedMult);
        float speedChange = targetSpeed - currentSpeed;

        // fix -> apply value modifier instead of set speed.
        actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
            RE::ActorValue::kSpeedMult, speedChange);
        logger::debug("Applied {} speed change (from {} to {})", speedChange, currentSpeed, targetSpeed);
    }

    void SlowMotionManager::RemoveSlowdown(RE::Actor* actor, SlowType type) {
        if (!actor) return;

        auto formID = actor->GetFormID();
        auto it = actorStates.find(formID);
        if (it == actorStates.end()) return;

        auto& state = it->second;

        switch (type) {
        case SlowType::Bow:
        case SlowType::Crossbow:
            state.bowSlowActive = false;
            break;
        case SlowType::CastLeft:
            state.castLeftActive = false;
            break;
        case SlowType::CastRight:
            state.castRightActive = false;
            break;
        case SlowType::DualCast:
            state.dualCastActive = false;
            break;
        }

        if (!state.castLeftActive || !state.castRightActive) {
            state.dualCastActive = false;
        }

        if (!IsActorSlowed(actor)) {
            float targetSpeed = 100.0f + state.baseSpeedDelta;
            float currentSpeed = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeedMult);
            float speedChange = targetSpeed - currentSpeed;

            actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
                RE::ActorValue::kSpeedMult, speedChange);
            logger::debug("Restored speed by {} (from {} to {})", speedChange, currentSpeed, targetSpeed);

            actorStates.erase(it);
        }
        else {
            SlowType activeType;
            float skillLevel = 0.0f;

            if (state.bowSlowActive) {
                activeType = SlowType::Bow;
                skillLevel = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kArchery);
            }
            else if (state.dualCastActive) {
                activeType = SlowType::DualCast;
                auto spell = actor->GetActorRuntimeData().selectedSpells[RE::Actor::SlotTypes::kLeftHand];
                if (!spell) spell = actor->GetActorRuntimeData().selectedSpells[RE::Actor::SlotTypes::kRightHand];
                if (spell) {
                    auto spellItem = spell->As<RE::SpellItem>();
                    if (spellItem) {
                        auto school = spellItem->GetAssociatedSkill();
                        skillLevel = (school != RE::ActorValue::kNone)
                            ? actor->AsActorValueOwner()->GetActorValue(school) : 50.0f;
                    }
                }
            }
            else if (state.castLeftActive) {
                activeType = SlowType::CastLeft;
                auto spell = actor->GetActorRuntimeData().selectedSpells[RE::Actor::SlotTypes::kLeftHand];
                if (spell) {
                    auto spellItem = spell->As<RE::SpellItem>();
                    if (spellItem) {
                        auto school = spellItem->GetAssociatedSkill();
                        skillLevel = (school != RE::ActorValue::kNone)
                            ? actor->AsActorValueOwner()->GetActorValue(school) : 50.0f;
                    }
                }
            }
            else if (state.castRightActive) {
                activeType = SlowType::CastRight;
                auto spell = actor->GetActorRuntimeData().selectedSpells[RE::Actor::SlotTypes::kRightHand];
                if (spell) {
                    auto spellItem = spell->As<RE::SpellItem>();
                    if (spellItem) {
                        auto school = spellItem->GetAssociatedSkill();
                        skillLevel = (school != RE::ActorValue::kNone)
                            ? actor->AsActorValueOwner()->GetActorValue(school) : 50.0f;
                    }
                }
            }

            float multiplier = CalculateSpeedMultiplier(skillLevel, activeType);
            float targetSpeed = (100.0f * multiplier) + state.baseSpeedDelta;
            float currentSpeed = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeedMult);
            float speedChange = targetSpeed - currentSpeed;

            actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
                RE::ActorValue::kSpeedMult, speedChange);
            logger::debug("Recalculated: changed speed by {} (from {} to {})", speedChange, currentSpeed, targetSpeed);
        }
    }

    void SlowMotionManager::ClearAllSlowdowns(RE::Actor* actor) {
        if (!actor) return;

        auto formID = actor->GetFormID();
        auto it = actorStates.find(formID);
        if (it == actorStates.end()) return;

        float targetSpeed = 100.0f + it->second.baseSpeedDelta;
        float currentSpeed = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeedMult);
        float speedToRestore = targetSpeed - currentSpeed;

        if (speedToRestore > 0) {
            actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
                RE::ActorValue::kSpeedMult, speedToRestore);
            logger::debug("Cleared all: restored by {} (from {} to {})", speedToRestore, currentSpeed, targetSpeed);
        }

        actorStates.erase(it);
    }

    void SlowMotionManager::ClearAll() {
        for (auto& [formID, state] : actorStates) {
            if (auto actor = RE::TESForm::LookupByID<RE::Actor>(formID)) {
                float targetSpeed = 100.0f + state.baseSpeedDelta;
                float currentSpeed = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeedMult);
                float speedToRestore = targetSpeed - currentSpeed;

                if (speedToRestore > 0) {
                    actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
                        RE::ActorValue::kSpeedMult, speedToRestore);
                }
            }
        }
        actorStates.clear();
        logger::debug("Cleared all slowdowns for all actors");
    }

    bool SlowMotionManager::IsActorSlowed(RE::Actor* actor) {
        auto it = actorStates.find(actor->GetFormID());
        if (it == actorStates.end()) return false;

        auto& state = it->second;
        return state.bowSlowActive || state.castLeftActive ||
            state.castRightActive || state.dualCastActive;
    }

    float SlowMotionManager::CalculateSpeedMultiplier(float skillLevel, SlowType type) {
        auto config = Config::GetSingleton();

        int tier = 0;
        if (skillLevel <= 25) tier = 0;
        else if (skillLevel <= 50) tier = 1;
        else if (skillLevel <= 75) tier = 2;
        else tier = 3;

        float mult = 1.0f;
        switch (type) {
        case SlowType::Bow:
            mult = config->bowMultipliers[tier];
            break;
        case SlowType::Crossbow:
            mult = config->crossbowMultipliers[tier];
            break;
        case SlowType::CastLeft:
        case SlowType::CastRight:
            mult = config->castMultipliers[tier];
            break;
        case SlowType::DualCast:
            mult = config->dualCastMultipliers[tier];
            break;
        }

        return mult;
    }
}