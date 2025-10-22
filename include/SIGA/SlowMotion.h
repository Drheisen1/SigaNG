#pragma once

#include <unordered_map>
#include <chrono>

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

        void ApplySlowdown(RE::Actor* actor, SlowType type, float skillLevel);
        void RemoveSlowdown(RE::Actor* actor, SlowType type);
        void ClearAllSlowdowns(RE::Actor* actor);
        void ClearAll();
        void CleanupInactiveStates();

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
            float baseSpeedDelta = 0.0f;  // Changed from originalSpeedMult
            std::chrono::steady_clock::time_point lastCastTime;
            float expectedSpeed = 100.0f;
        };

        std::unordered_map<RE::FormID, ActorSlowState> actorStates;

        float CalculateSpeedMultiplier(float skillLevel, SlowType type);
        void ApplySpeedMultiplier(RE::Actor* actor, float mult);
    };
}