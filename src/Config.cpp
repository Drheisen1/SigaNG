#include "SIGA/Config.h"
#include <SimpleIni.h>

namespace SIGA {
    std::filesystem::path Config::GetConfigPath() {
        auto path = std::filesystem::current_path() / "Data" / "SKSE" / "Plugins" / "SIGA.ini";
        return path;
    }

    void Config::Load() {
        CSimpleIniA ini;
        ini.SetUnicode();

        auto path = GetConfigPath();

        if (ini.LoadFile(path.string().c_str()) < 0) {
            logger::warn("Config file not found at {}, creating with defaults", path.string());
            Save();
            return;
        }

        // General settings
        enabled = ini.GetBoolValue("General", "bEnabled", true);
        applyToNPCs = ini.GetBoolValue("General", "bApplyToNPCs", false);
        applySlowdownCastingToNPCsOnly = ini.GetBoolValue("General", "bApplySlowdownCastingToNPCsOnly", false);
        logLevel = ini.GetLongValue("General", "iLogLevel", 2);

        // Enable/Disable specific debuffs
        enableBowDebuff = ini.GetBoolValue("General", "bEnableBowDebuff", true);
        enableCrossbowDebuff = ini.GetBoolValue("General", "bEnableCrossbowDebuff", true);
        enableCastDebuff = ini.GetBoolValue("General", "bEnableCastDebuff", true);
        enableDualCastDebuff = ini.GetBoolValue("General", "bEnableDualCastDebuff", true);

        // Bow multipliers
        bowMultipliers[0] = static_cast<float>(ini.GetDoubleValue("Bow", "fNoviceMultiplier", 0.5));
        bowMultipliers[1] = static_cast<float>(ini.GetDoubleValue("Bow", "fApprenticeMultiplier", 0.6));
        bowMultipliers[2] = static_cast<float>(ini.GetDoubleValue("Bow", "fExpertMultiplier", 0.7));
        bowMultipliers[3] = static_cast<float>(ini.GetDoubleValue("Bow", "fMasterMultiplier", 0.8));

        // Crossbow multipliers
        crossbowMultipliers[0] = static_cast<float>(ini.GetDoubleValue("Crossbow", "fNoviceMultiplier", 0.5));
        crossbowMultipliers[1] = static_cast<float>(ini.GetDoubleValue("Crossbow", "fApprenticeMultiplier", 0.6));
        crossbowMultipliers[2] = static_cast<float>(ini.GetDoubleValue("Crossbow", "fExpertMultiplier", 0.7));
        crossbowMultipliers[3] = static_cast<float>(ini.GetDoubleValue("Crossbow", "fMasterMultiplier", 0.8));

        // Cast multipliers
        castMultipliers[0] = static_cast<float>(ini.GetDoubleValue("Cast", "fNoviceMultiplier", 0.5));
        castMultipliers[1] = static_cast<float>(ini.GetDoubleValue("Cast", "fApprenticeMultiplier", 0.6));
        castMultipliers[2] = static_cast<float>(ini.GetDoubleValue("Cast", "fExpertMultiplier", 0.7));
        castMultipliers[3] = static_cast<float>(ini.GetDoubleValue("Cast", "fMasterMultiplier", 0.8));

        // Dual cast multipliers
        dualCastMultipliers[0] = static_cast<float>(ini.GetDoubleValue("DualCast", "fNoviceMultiplier", 0.4));
        dualCastMultipliers[1] = static_cast<float>(ini.GetDoubleValue("DualCast", "fApprenticeMultiplier", 0.5));
        dualCastMultipliers[2] = static_cast<float>(ini.GetDoubleValue("DualCast", "fExpertMultiplier", 0.6));
        dualCastMultipliers[3] = static_cast<float>(ini.GetDoubleValue("DualCast", "fMasterMultiplier", 0.7));

        logger::info("Config loaded successfully from {}", path.string());
    }

    void Config::Save() {
        CSimpleIniA ini;
        ini.SetUnicode();

        // General section
        ini.SetValue("General", nullptr, "; SIGA - Slow Motion Combat Plugin");
        ini.SetBoolValue("General", "bEnabled", enabled);
        ini.SetValue("General", nullptr, "; Apply slowdown to NPCs in combat");
        ini.SetBoolValue("General", "bApplyToNPCs", applyToNPCs);
        ini.SetValue("General", nullptr, "; Apply casting slowdown to NPCs only (not player)");
        ini.SetBoolValue("General", "bApplySlowdownCastingToNPCsOnly", applySlowdownCastingToNPCsOnly);
        ini.SetValue("General", nullptr, "; Log level: 0=trace, 1=debug, 2=info, 3=warn, 4=error, 5=critical");
        ini.SetLongValue("General", "iLogLevel", logLevel);

        ini.SetValue("General", nullptr, "; Enable/Disable specific slowdown types");
        ini.SetBoolValue("General", "bEnableBowDebuff", enableBowDebuff);
        ini.SetBoolValue("General", "bEnableCrossbowDebuff", enableCrossbowDebuff);
        ini.SetBoolValue("General", "bEnableCastDebuff", enableCastDebuff);
        ini.SetBoolValue("General", "bEnableDualCastDebuff", enableDualCastDebuff);

        // Bow section
        ini.SetValue("Bow", nullptr, "; Bow slowdown multipliers by skill level");
        ini.SetDoubleValue("Bow", "fNoviceMultiplier", bowMultipliers[0]);
        ini.SetDoubleValue("Bow", "fApprenticeMultiplier", bowMultipliers[1]);
        ini.SetDoubleValue("Bow", "fExpertMultiplier", bowMultipliers[2]);
        ini.SetDoubleValue("Bow", "fMasterMultiplier", bowMultipliers[3]);

        // Crossbow section
        ini.SetValue("Crossbow", nullptr, "; Crossbow slowdown multipliers by skill level");
        ini.SetDoubleValue("Crossbow", "fNoviceMultiplier", crossbowMultipliers[0]);
        ini.SetDoubleValue("Crossbow", "fApprenticeMultiplier", crossbowMultipliers[1]);
        ini.SetDoubleValue("Crossbow", "fExpertMultiplier", crossbowMultipliers[2]);
        ini.SetDoubleValue("Crossbow", "fMasterMultiplier", crossbowMultipliers[3]);

        // Cast section
        ini.SetValue("Cast", nullptr, "; Magic casting slowdown multipliers by skill level");
        ini.SetDoubleValue("Cast", "fNoviceMultiplier", castMultipliers[0]);
        ini.SetDoubleValue("Cast", "fApprenticeMultiplier", castMultipliers[1]);
        ini.SetDoubleValue("Cast", "fExpertMultiplier", castMultipliers[2]);
        ini.SetDoubleValue("Cast", "fMasterMultiplier", castMultipliers[3]);

        // Dual cast section
        ini.SetValue("DualCast", nullptr, "; Dual casting slowdown multipliers by skill level");
        ini.SetDoubleValue("DualCast", "fNoviceMultiplier", dualCastMultipliers[0]);
        ini.SetDoubleValue("DualCast", "fApprenticeMultiplier", dualCastMultipliers[1]);
        ini.SetDoubleValue("DualCast", "fExpertMultiplier", dualCastMultipliers[2]);
        ini.SetDoubleValue("DualCast", "fMasterMultiplier", dualCastMultipliers[3]);

        auto path = GetConfigPath();
        std::filesystem::create_directories(path.parent_path());
        ini.SaveFile(path.string().c_str());
        logger::info("Config saved to {}", path.string());
    }
}