#pragma once

#include <string>

#include "logger.hpp"

struct ModConfig
{
    float first_person_sensitivity_scale;
    float first_person_ads_scale;
    float third_person_sensitivity_scale;
    float third_person_ads_scale;
    bool first_person_fov_sensitivity_scaling;
    bool show_menu_at_start_up;
    bool user_interface_enabled;
    LogLevel log_level;
};

ModConfig default_config();
ModConfig load_config();
bool save_config(const ModConfig *Config);
