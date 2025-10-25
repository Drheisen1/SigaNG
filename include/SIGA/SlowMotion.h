#pragma once

#include <unordered_map>
#include <mutex>

namespace SIGA {
    enum class SlowType {
        Bow,
        Crossbow,
        CastLeft,
        CastRight,
        DualCast
    };

    class SlowMotionManager {
    public:
        static SlowMotionManager* GetSingleton();

        // Initialize spell lookups
        bool Initialize();

        void ApplySlowdown(RE::Actor* actor, SlowType type, float skillLevel);
        void RemoveSlowdown(RE::Actor* actor, SlowType type);
        void ClearAllSlowdowns(RE::Actor* actor);
        void ClearAll();

        bool IsActorSlowed(RE::Actor* actor);

    private:
        SlowMotionManager() = default;
        SlowMotionManager(const SlowMotionManager&) = delete;
        SlowMotionManager(SlowMotionManager&&) = delete;

        struct ActorSlowState {
            bool bowSlowActive = false;
            bool castLeftActive = false;
            bool castRightActive = false;
            bool dualCastActive = false;
        };

        std::unordered_map<RE::FormID, ActorSlowState> actorStates;
        mutable std::mutex actorStatesMutex;

        // Cached spell pointers
        RE::SpellItem* bowDebuffSpell = nullptr;
        RE::SpellItem* castingDebuffSpell = nullptr;
        RE::SpellItem* dualCastDebuffSpell = nullptr;
        RE::SpellItem* crossbowDebuffSpell = nullptr;

        float CalculateMagnitude(float skillLevel, SlowType type);
        void ApplySpellWithMagnitude(RE::Actor* actor, RE::SpellItem* spell, float magnitude);
        void RemoveSpell(RE::Actor* actor, RE::SpellItem* spell);
        bool IsActorSlowedInternal(RE::FormID formID) const;
    };
}