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
        // OPTIMIZATION: Use try_emplace instead of operator[] to avoid double hash lookup
        auto [it, inserted] = actorStates.try_emplace(formID);
        auto& state = it->second;

        // Only capture delta if this is the FIRST slowdown
        if (!IsActorSlowed(actor)) {
            float currentSpeed = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeedMult);

            // SAFEGUARD - Reject obviously corrupted speeds
            if (currentSpeed < 20.0f) {
                logger::warn("Detected corrupted speed ({}) - using base 100", currentSpeed);
                state.baseSpeedDelta = 0.0f;
            }
            else {
                state.baseSpeedDelta = currentSpeed - 100.0f;
                logger::debug("Captured speed delta: {} (current speed: {})", state.baseSpeedDelta, currentSpeed);
            }
        }
        else {
            // Already slowed - keep existing delta, DON'T recapture
            logger::debug("Already slowed, keeping cached delta: {}", state.baseSpeedDelta);
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

        actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
            RE::ActorValue::kSpeedMult, speedChange);
        logger::debug("Applied {} speed change (from {} to {})", speedChange, currentSpeed, targetSpeed);

        state.expectedSpeed = targetSpeed;
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

            state.expectedSpeed = targetSpeed;
            state.lastCastTime = std::chrono::steady_clock::now();
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

    void SlowMotionManager::CleanupInactiveStates() {
        auto now = std::chrono::steady_clock::now();

        for (auto it = actorStates.begin(); it != actorStates.end();) {
            auto formID = it->first;
            auto& state = it->second;
            auto actor = RE::TESForm::LookupByID<RE::Actor>(formID);

            // Only check states where NO slowdowns are active
            if (!state.bowSlowActive && !state.castLeftActive &&
                !state.castRightActive && !state.dualCastActive) {

                bool shouldClear = false;

                // Idle for 3+ seconds
                auto idleSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                    now - state.lastCastTime).count();
                if (idleSeconds >= 3) {
                    shouldClear = true;
                }

                // External speed change detected
                if (actor) {
                    float actualSpeed = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeedMult);
                    float speedDiff = std::abs(actualSpeed - state.expectedSpeed);

                    // If speed changed by more than 0.1, something external modified it
                    if (speedDiff > 0.1f) {
                        logger::debug("External speed change detected (expected: {}, actual: {}) - clearing delta",
                            state.expectedSpeed, actualSpeed);
                        shouldClear = true;
                    }
                }

                if (shouldClear) {
                    it = actorStates.erase(it);
                    continue;
                }
            }
            ++it;
        }
    }

    void SlowMotionManager::ClearAllSlowdowns(RE::Actor* actor) {
        if (!actor) return;

        auto formID = actor->GetFormID();
        auto it = actorStates.find(formID);
        if (it == actorStates.end()) return;

        float targetSpeed = 100.0f + it->second.baseSpeedDelta;
        float currentSpeed = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeedMult);
        float speedChange = targetSpeed - currentSpeed;

        actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
            RE::ActorValue::kSpeedMult, speedChange);
        logger::debug("Cleared all: changed speed by {} (from {} to {})", speedChange, currentSpeed, targetSpeed);

        actorStates.erase(it);
    }

    void SlowMotionManager::ClearAll() {
        for (auto& [formID, state] : actorStates) {
            if (auto actor = RE::TESForm::LookupByID<RE::Actor>(formID)) {
                float targetSpeed = 100.0f + state.baseSpeedDelta;
                float currentSpeed = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kSpeedMult);
                float speedChange = targetSpeed - currentSpeed;

                actor->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage,
                    RE::ActorValue::kSpeedMult, speedChange);
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