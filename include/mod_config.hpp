#pragma once

#include <string>

#include "logger.hpp"

struct ModConfig
{
    float FirstPersonSensitivity;
    float FirstPersonADSScale;
    float ThirdPersonSensitivity;
    float ThirdPersonADSScale;
    bool FirstPersonFovSensitivityScaling;
    bool ShowMenuAtStartUp;
    bool UserInterfaceEnabled;
    LogLevel LogLevel;
};

ModConfig DefaultConfig();
ModConfig LoadConfig();
bool SaveConfig(const ModConfig *Config);
