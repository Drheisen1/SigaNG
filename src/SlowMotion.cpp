#include "SIGA/SlowMotion.h"
#include "SIGA/Config.h"

namespace SIGA {

    SlowMotionManager* SlowMotionManager::GetSingleton() {
        static SlowMotionManager singleton;
        return &singleton;
    }

    bool SlowMotionManager::Initialize() {
        auto config = Config::GetSingleton();
        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            logger::error("Failed to get TESDataHandler");
            return false;
        }

        const char* pluginName = config->pluginName.c_str();

        // Look up spells from the plugin
        bowDebuffSpell = dataHandler->LookupForm<RE::SpellItem>(config->bowDebuffSpellID, pluginName);
        castingDebuffSpell = dataHandler->LookupForm<RE::SpellItem>(config->castingDebuffSpellID, pluginName);
        dualCastDebuffSpell = dataHandler->LookupForm<RE::SpellItem>(config->dualCastDebuffSpellID, pluginName);
        crossbowDebuffSpell = dataHandler->LookupForm<RE::SpellItem>(config->crossbowDebuffSpellID, pluginName);

        // Verify all spells loaded
        bool success = true;
        if (!bowDebuffSpell) {
            logger::error("Failed to load bow debuff spell (0x{:X})", config->bowDebuffSpellID);
            success = false;
        }
        if (!castingDebuffSpell) {
            logger::error("Failed to load casting debuff spell (0x{:X})", config->castingDebuffSpellID);
            success = false;
        }
        if (!dualCastDebuffSpell) {
            logger::error("Failed to load dual cast debuff spell (0x{:X})", config->dualCastDebuffSpellID);
            success = false;
        }
        if (!crossbowDebuffSpell) {
            logger::error("Failed to load crossbow debuff spell (0x{:X})", config->crossbowDebuffSpellID);
            success = false;
        }

        if (success) {
            logger::info("All debuff spells loaded successfully");
        }

        return success;
    }

    void SlowMotionManager::ApplySlowdown(RE::Actor* actor, SlowType type, float skillLevel) {
        if (!actor) {
            logger::warn("ApplySlowdown called with null actor");
            return;
        }

        std::lock_guard<std::mutex> lock(actorStatesMutex);

        auto formID = actor->GetFormID();
        auto [it, inserted] = actorStates.try_emplace(formID);
        auto& state = it->second;

        logger::debug("ApplySlowdown: type={}, skillLevel={}", static_cast<int>(type), skillLevel);

        // Determine which spell to use
        RE::SpellItem* spellToApply = nullptr;
        bool isDualCast = false;

        switch (type) {
        case SlowType::Bow:
            state.bowSlowActive = true;
            spellToApply = bowDebuffSpell;
            break;
        case SlowType::Crossbow:
            state.bowSlowActive = true;
            spellToApply = crossbowDebuffSpell;
            break;
        case SlowType::CastLeft:
            state.castLeftActive = true;
            spellToApply = castingDebuffSpell;
            break;
        case SlowType::CastRight:
            state.castRightActive = true;
            spellToApply = castingDebuffSpell;
            break;
        }

        // Check for dual cast
        if (state.castLeftActive && state.castRightActive) {
            state.dualCastActive = true;
            isDualCast = true;
            spellToApply = dualCastDebuffSpell;
            type = SlowType::DualCast;
            logger::debug("Dual casting detected!");
        }

        if (!spellToApply) {
            logger::error("No spell found for slowdown type {}", static_cast<int>(type));
            return;
        }

        // Calculate magnitude based on skill level
        float magnitude = CalculateMagnitude(skillLevel, type);

        // Apply the spell with the calculated magnitude
        logger::debug("Applying {} to actor (magnitude: {})", spellToApply->GetName(), magnitude);
        ApplySpellWithMagnitude(actor, spellToApply, magnitude);
    }

    void SlowMotionManager::RemoveSlowdown(RE::Actor* actor, SlowType type) {
        if (!actor) return;

        std::lock_guard<std::mutex> lock(actorStatesMutex);

        auto formID = actor->GetFormID();
        auto it = actorStates.find(formID);
        if (it == actorStates.end()) return;

        auto& state = it->second;

        // Update state flags
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

        // If both cast hands are released, disable dual cast
        if (!state.castLeftActive || !state.castRightActive) {
            state.dualCastActive = false;
        }

        // Remove all active spells
        if (state.bowSlowActive) {
            RemoveSpell(actor, bowDebuffSpell);
            RemoveSpell(actor, crossbowDebuffSpell);
        }
        if (state.dualCastActive) {
            RemoveSpell(actor, dualCastDebuffSpell);
        } else if (state.castLeftActive || state.castRightActive) {
            RemoveSpell(actor, castingDebuffSpell);
        } else {
            // No casting active, remove casting spells
            RemoveSpell(actor, castingDebuffSpell);
            RemoveSpell(actor, dualCastDebuffSpell);
        }

        // If no slowdowns are active, clean up state
        if (!IsActorSlowedInternal(formID)) {
            actorStates.erase(it);
            logger::debug("Removed all slowdowns for actor");
        }
    }

    void SlowMotionManager::ClearAllSlowdowns(RE::Actor* actor) {
        if (!actor) return;

        std::lock_guard<std::mutex> lock(actorStatesMutex);

        auto formID = actor->GetFormID();
        auto it = actorStates.find(formID);
        if (it == actorStates.end()) return;

        // Remove all spell effects
        RemoveSpell(actor, bowDebuffSpell);
        RemoveSpell(actor, crossbowDebuffSpell);
        RemoveSpell(actor, castingDebuffSpell);
        RemoveSpell(actor, dualCastDebuffSpell);

        actorStates.erase(it);
        logger::debug("Cleared all slowdowns for actor");
    }

    void SlowMotionManager::ClearAll() {
        std::lock_guard<std::mutex> lock(actorStatesMutex);

        for (auto& [formID, state] : actorStates) {
            if (auto actor = RE::TESForm::LookupByID<RE::Actor>(formID)) {
                RemoveSpell(actor, bowDebuffSpell);
                RemoveSpell(actor, crossbowDebuffSpell);
                RemoveSpell(actor, castingDebuffSpell);
                RemoveSpell(actor, dualCastDebuffSpell);
            }
        }
        actorStates.clear();
        logger::debug("Cleared all slowdowns for all actors");
    }

    bool SlowMotionManager::IsActorSlowed(RE::Actor* actor) {
        if (!actor) return false;

        std::lock_guard<std::mutex> lock(actorStatesMutex);
        return IsActorSlowedInternal(actor->GetFormID());
    }

    bool SlowMotionManager::IsActorSlowedInternal(RE::FormID formID) const {
        auto it = actorStates.find(formID);
        if (it == actorStates.end()) return false;

        auto& state = it->second;
        return state.bowSlowActive || state.castLeftActive ||
            state.castRightActive || state.dualCastActive;
    }

    float SlowMotionManager::CalculateMagnitude(float skillLevel, SlowType type) {
        auto config = Config::GetSingleton();

        // Determine skill tier
        int tier = 0;
        if (skillLevel <= 25) tier = 0;
        else if (skillLevel <= 50) tier = 1;
        else if (skillLevel <= 75) tier = 2;
        else tier = 3;

        // Get multiplier from config
        float multiplier = 1.0f;
        switch (type) {
        case SlowType::Bow:
            multiplier = config->bowMultipliers[tier];
            break;
        case SlowType::Crossbow:
            multiplier = config->crossbowMultipliers[tier];
            break;
        case SlowType::CastLeft:
        case SlowType::CastRight:
            multiplier = config->castMultipliers[tier];
            break;
        case SlowType::DualCast:
            multiplier = config->dualCastMultipliers[tier];
            break;
        }

        // Convert multiplier to magnitude
        // multiplier 0.5 = 50% speed = need to REDUCE by 50 = magnitude 50
        // multiplier 0.7 = 70% speed = need to REDUCE by 30 = magnitude 30
        float magnitude = 100.0f - (multiplier * 100.0f);

        logger::debug("Calculated magnitude: {} (multiplier: {}, tier: {})", magnitude, multiplier, tier);
        return magnitude;
    }

    void SlowMotionManager::ApplySpellWithMagnitude(RE::Actor* actor, RE::SpellItem* spell, float magnitude) {
        if (!actor || !spell) return;

        // First, modify the spell's magnitude
        if (spell->effects.size() > 0) {
            auto& effect = spell->effects[0];
            if (effect) {
                effect->effectItem.magnitude = magnitude;
                logger::debug("Set spell effect magnitude to {}", effect->effectItem.magnitude);
            }
        }

        // Cast the spell on the actor
        auto caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        if (caster) {
            caster->CastSpellImmediate(
                spell,                    // spell
                false,                    // no hit effect art
                actor,                    // target
                1.0f,                     // effectiveness
                false,                    // hostile effectiveness only
                magnitude,                // magnitude override
                nullptr                   // blame actor
            );
            logger::debug("Cast spell {} on actor", spell->GetName());
        } else {
            logger::warn("Failed to get magic caster for actor");
        }
    }

    void SlowMotionManager::RemoveSpell(RE::Actor* actor, RE::SpellItem* spell) {
        if (!actor || !spell) return;

        // Dispel the effect
        auto magicTarget = actor->GetMagicTarget();
        if (magicTarget) {
            // Get a null handle for the caster
            RE::BSPointerHandle<RE::Actor> nullHandle;
            magicTarget->DispelEffect(spell, nullHandle);
            logger::debug("Dispelled spell {} from actor", spell->GetName());
        }
    }
}
