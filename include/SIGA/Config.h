#pragma once
#include <array>
#include <filesystem>

namespace SIGA {
    class Config {
    public:
        static Config* GetSingleton() {
            static Config singleton;
            return &singleton;
        }

        void Load();
        void Save();

        // General settings
        bool enabled = true;
        bool applyToNPCs = false;
        int logLevel = 2;  // 0=trace, 1=debug, 2=info, 3=warn, 4=error, 5=critical

        // Enable/Disable specific debuffs
        bool enableBowDebuff = true;
        bool enableCrossbowDebuff = true;
        bool enableCastDebuff = true;
        bool enableDualCastDebuff = true;

        // Bow multipliers (Novice/Apprentice/Expert/Master)
        std::array<float, 4> bowMultipliers = { 0.5f, 0.6f, 0.7f, 0.8f };
        std::array<float, 4> crossbowMultipliers = { 0.5f, 0.6f, 0.7f, 0.8f };
        std::array<float, 4> castMultipliers = { 0.5f, 0.6f, 0.7f, 0.8f };
        std::array<float, 4> dualCastMultipliers = { 0.4f, 0.5f, 0.6f, 0.7f };

    private:
        Config() = default;
        Config(const Config&) = delete;
        Config(Config&&) = delete;

        static std::filesystem::path GetConfigPath();
    };
}