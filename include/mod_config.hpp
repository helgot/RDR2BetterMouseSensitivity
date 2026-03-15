#pragma once

#include <string>

#include "logger.hpp"

struct ModConfig
{
    float FirstPersonSensitivity;
    float FirstPersonAimScale;
    float ThirdPersonSensitivity;
    float ThirdPersonAimScale;
    bool FirstPersonFovSensitivityScaling;
    bool ShowMenuAtStartUp;
    bool UserInterfaceEnabled;
    LogLevel LogLevel;
};

ModConfig DefaultConfig();
ModConfig LoadConfig();
bool SaveConfig(const ModConfig *Config);
