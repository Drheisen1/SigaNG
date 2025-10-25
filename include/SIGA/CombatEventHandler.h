#pragma once
#include <unordered_set>  
#include <mutex>          

namespace SIGA {
    class CombatEventHandler : public RE::BSTEventSink<RE::TESCombatEvent> {
    public:
        static CombatEventHandler* GetSingleton() {
            static CombatEventHandler singleton;
            return &singleton;
        }

        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESCombatEvent* a_event,
            RE::BSTEventSource<RE::TESCombatEvent>* a_eventSource) override;

    private:
        CombatEventHandler() = default;
        CombatEventHandler(const CombatEventHandler&) = delete;
        CombatEventHandler(CombatEventHandler&&) = delete;
        ~CombatEventHandler() = default;

        std::unordered_set<RE::FormID> registeredNPCs;
        std::mutex registrationMutex;
    };
}