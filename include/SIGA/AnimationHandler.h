#pragma once

namespace SIGA {
    class AnimationEventHandler : public RE::BSTEventSink<RE::BSAnimationGraphEvent> {
    public:
        static AnimationEventHandler* GetSingleton();


        RE::BSEventNotifyControl ProcessEvent(
            const RE::BSAnimationGraphEvent* a_event,
            RE::BSTEventSource<RE::BSAnimationGraphEvent>* a_eventSource) override;

    private:
        AnimationEventHandler() = default;
        AnimationEventHandler(const AnimationEventHandler&) = delete;
        AnimationEventHandler(AnimationEventHandler&&) = delete;

        void OnBowDrawn(RE::Actor* actor);
        void OnBeginCastLeft(RE::Actor* actor);
        void OnBeginCastRight(RE::Actor* actor);
        void OnCastRelease(RE::Actor* actor);
        void OnAttackStop(RE::Actor* actor);

        float GetMagicSkillLevel(RE::Actor* actor, RE::MagicItem* spell);
        bool SpellModifiesSpeed(RE::MagicItem* spell);  // <-- ADD THIS
    };
}