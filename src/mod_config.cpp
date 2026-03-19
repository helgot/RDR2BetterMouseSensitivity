#include "mod_config.hpp"

#include <string.h>

static const char *kConfigFileName = "RDR2BetterMouseSensitivity.ini";

ModConfig default_config()
{
    ModConfig config;
    config.first_person_sensitivity_scale = 1.0f;
    config.first_person_ads_scale = 1.0f;
    config.third_person_sensitivity_scale = 1.0f;
    config.third_person_ads_scale = 1.0f;
    config.first_person_fov_sensitivity_scaling = true;
    config.show_menu_at_start_up = true;
    config.user_interface_enabled = true;
    config.log_level = LOG_LEVEL_INFO;
    return config;
}

ModConfig load_config()
{
    ModConfig CONFIG = default_config();
    FILE *file = fopen(kConfigFileName, "r");

    if (!file)
    {
        LOG_ERROR("%s: Failed to read config file, using default settings.",
                  __func__);
        return CONFIG;
    }

    char line[128];
    char key[64];
    char value[64];

    while (fgets(line, sizeof(line), file))
    {
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2)
        {
            if (strcmp(key, "first_person_sensitivity_scale") == 0)
            {
                CONFIG.first_person_sensitivity_scale = strtof(value, NULL);
            }
            else if (strcmp(key, "first_person_ads_scale") == 0)
            {
                CONFIG.first_person_ads_scale = strtof(value, NULL);
            }
            else if (strcmp(key, "third_person_sensitivity_scale") == 0)
            {
                CONFIG.third_person_sensitivity_scale = strtof(value, NULL);
            }
            else if (strcmp(key, "third_person_ads_scale") == 0)
            {
                CONFIG.third_person_ads_scale = strtof(value, NULL);
            }
            else if (strcmp(key, "first_person_fov_sensitivity_scaling") == 0)
            {
                CONFIG.first_person_fov_sensitivity_scaling =
                    (strcmp(value, "true") == 0);
            }
            else if (strcmp(key, "show_menu_at_start_up") == 0)
            {
                CONFIG.show_menu_at_start_up = (strcmp(value, "true") == 0);
            }
            else if (strcmp(key, "user_interface_enabled") == 0)
            {
                CONFIG.user_interface_enabled = (strcmp(value, "true") == 0);
            }
            else if (strcmp(key, "log_level") == 0)
            {
                LogLevel level = string_to_log_level(value);
                CONFIG.log_level =
                    (level == LOG_LEVEL_UNKNOWN) ? LOG_LEVEL_INFO : level;
            }
        }
    }
    fclose(file);
    return CONFIG;
}

bool save_config(const ModConfig *CONFIG)
{
    FILE *file = fopen(kConfigFileName, "w");

    if (!file)
    {
        return false;
    }

    fprintf(file, "first_person_sensitivity_scale=%f\n",
            CONFIG->first_person_sensitivity_scale);
    fprintf(file, "first_person_ads_scale=%f\n",
            CONFIG->first_person_ads_scale);
    fprintf(file, "third_person_sensitivity_scale=%f\n",
            CONFIG->third_person_sensitivity_scale);
    fprintf(file, "third_person_ads_scale=%f\n",
            CONFIG->third_person_ads_scale);
    fprintf(file, "first_person_fov_sensitivity_scaling=%s\n",
            CONFIG->first_person_fov_sensitivity_scaling ? "true" : "false");
    fprintf(file, "show_menu_at_start_up=%s\n",
            CONFIG->show_menu_at_start_up ? "true" : "false");
    fprintf(file, "user_interface_enabled=%s\n",
            CONFIG->user_interface_enabled ? "true" : "false");
    fprintf(file, "log_level=%s\n", log_level_to_string(CONFIG->log_level));
    fclose(file);

    return true;
}