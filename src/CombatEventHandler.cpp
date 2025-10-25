#include "SIGA/CombatEventHandler.h"
#include "SIGA/AnimationHandler.h"
#include "SIGA/Config.h"

namespace SIGA {

    RE::BSEventNotifyControl CombatEventHandler::ProcessEvent(
        const RE::TESCombatEvent* a_event,
        RE::BSTEventSource<RE::TESCombatEvent>* a_eventSource)
    {
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto config = Config::GetSingleton();
        if (!config->applyToNPCs) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Get the actor entering/leaving combat
        auto actorPtr = a_event->actor.get();
        if (!actorPtr || actorPtr->IsPlayerRef()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto actor = actorPtr->As<RE::Actor>();
        if (!actor) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // Check if entering combat
        if (a_event->newState.underlying() == 1) {
            std::lock_guard<std::mutex> lock(registrationMutex);

            auto formID = actor->GetFormID();

            // Check if already registered
            if (registeredNPCs.find(formID) != registeredNPCs.end()) {
                return RE::BSEventNotifyControl::kContinue;
            }

            // Try to register animation events
            if (actor->AddAnimationGraphEventSink(AnimationEventHandler::GetSingleton())) {
                registeredNPCs.insert(formID);
                logger::debug("Registered animation events for NPC: {} (FormID: {:X})",
                    actor->GetName(), formID);
            }
            else {
                logger::debug("Failed to register for NPC: {}", actor->GetName());
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }

}