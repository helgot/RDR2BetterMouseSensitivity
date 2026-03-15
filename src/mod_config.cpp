#include "mod_config.hpp"

#include <string.h>

static const char *kConfigFileName = "RDR2BetterMouseSensitivity.ini";

ModConfig DefaultConfig()
{
    ModConfig Config;
    Config.FirstPersonSensitivity = 1.0f;
    Config.FirstPersonAimScale = 1.0f;
    Config.ThirdPersonSensitivity = 1.0f;
    Config.ThirdPersonAimScale = 1.0f;
    Config.FirstPersonFovSensitivityScaling = false;
    Config.ShowMenuAtStartUp = true;
    Config.UserInterfaceEnabled = true;
    Config.LogLevel = LOG_LEVEL_INFO;
    return Config;
}

ModConfig LoadConfig()
{
    ModConfig Config = DefaultConfig();
    FILE *File = fopen(kConfigFileName, "r");

    if (!File)
    {
        LOG_ERROR("%s: Failed to read config file, using default settings.",
                  __func__);
        return Config;
    }

    char Line[128];
    char Key[64];
    char Value[64];

    while (fgets(Line, sizeof(Line), File))
    {
        if (sscanf(Line, "%63[^=]=%63s", Key, Value) == 2)
        {
            if (strcmp(Key, "FirstPersonSensitivity") == 0)
            {
                Config.FirstPersonSensitivity = strtof(Value, NULL);
            }
            else if (strcmp(Key, "FirstPersonAimScale") == 0)
            {
                Config.FirstPersonAimScale = strtof(Value, NULL);
            }
            else if (strcmp(Key, "ThirdPersonSensitivity") == 0)
            {
                Config.ThirdPersonSensitivity = strtof(Value, NULL);
            }
            else if (strcmp(Key, "ThirdPersonAimScale") == 0)
            {
                Config.ThirdPersonAimScale = strtof(Value, NULL);
            }
            else if (strcmp(Key, "FirstPersonFovSensitivityScaling") == 0)
            {
                Config.FirstPersonFovSensitivityScaling =
                    (strcmp(Value, "true") == 0);
            }
            else if (strcmp(Key, "ShowMenuAtStartUp") == 0)
            {
                Config.ShowMenuAtStartUp = (strcmp(Value, "true") == 0);
            }
            else if (strcmp(Key, "UserInterfaceEnabled") == 0)
            {
                Config.UserInterfaceEnabled = (strcmp(Value, "true") == 0);
            }
            else if (strcmp(Key, "LogLevel") == 0)
            {
                LogLevel Level = StringToLogLevel(Value);
                Config.LogLevel =
                    (Level == LOG_LEVEL_UNKNOWN) ? LOG_LEVEL_INFO : Level;
            }
        }
    }
    fclose(File);
    return Config;
}

bool SaveConfig(const ModConfig *Config)
{
    FILE *File = fopen(kConfigFileName, "w");

    if (!File)
    {
        return false;
    }

    fprintf(File, "FirstPersonSensitivity=%f\n",
            Config->FirstPersonSensitivity);
    fprintf(File, "FirstPersonAimScale=%f\n", Config->FirstPersonAimScale);
    fprintf(File, "ThirdPersonSensitivity=%f\n",
            Config->ThirdPersonSensitivity);
    fprintf(File, "ThirdPersonAimScale=%f\n", Config->ThirdPersonAimScale);
    fprintf(File, "FirstPersonFovSensitivityScaling=%s\n",
            Config->FirstPersonFovSensitivityScaling ? "true" : "false");
    fprintf(File, "ShowMenuAtStartUp=%s\n",
            Config->ShowMenuAtStartUp ? "true" : "false");
    fprintf(File, "UserInterfaceEnabled=%s\n",
            Config->UserInterfaceEnabled ? "true" : "false");
    fprintf(File, "LogLevel=%s\n", LogLevelToString(Config->LogLevel));
    fclose(File);

    return true;
}