#include "mod_main.hpp"

#include <MinHook.h>
#include <ShlObj.h>
#include <windows.h>

#include "game_hook.hpp"
#include "helper.hpp"
#include "logger.hpp"
#include "mod_config.hpp"
#include "renderer_hook_d3d12.hpp"
#include "renderer_hook_vulkan.hpp"

#include "version.h"

ModConfig CONFIG;
extern bool MENU_VISIBLE;

static GraphicsAPI CONFIGURED_API = GRAPHICS_API_UNKNOWN;

void init_mod()
{
    init_logger(LOG_LEVEL_DEBUG);
    CONFIG = load_config();
    save_config(&CONFIG);

    MENU_VISIBLE = CONFIG.show_menu_at_start_up;

    OutputDebugStringA("Mouse Sensitivity Mod initialized\n");
    // Log level test:
    LOG_DEBUG("Mouse Sensitivity Mod initialized.");
    LOG_INFO("Mouse Sensitivity Mod initialized.");
    LOG_WARNING("Mouse Sensitivity Mod initialized.");
    LOG_ERROR("Mouse Sensitivity Mod initialized.");

    char VersionInfo[100];
    sprintf(VersionInfo, "RDR2 Mouse Sensitivity by Helgot, Version: %s",
            APP_GIT_DESCRIBE);
    LOG_INFO(VersionInfo);

    if (MH_Initialize() != MH_OK)
    {
        LOG_ERROR("Failed to initialize MinHook.");
        return;
    }
    else
    {
        LOG_INFO("Initialized MinHook.");
    }

    install_game_hooks();

    if (CONFIG.user_interface_enabled)
    {
        CONFIGURED_API = get_graphics_api_from_settings_file();
        switch (CONFIGURED_API)
        {
        case GRAPHICS_API_VULKAN: {
            vulkan_hook_install();
        }
        break;
        case GRAPHICS_API_DX12: {
            d3d12_hook_install();
        }
        break;
        default:
            break;
        }
    }
}

void shutdown_mod()
{

    if (CONFIG.user_interface_enabled)
    {
        switch (CONFIGURED_API)
        {
        case GRAPHICS_API_VULKAN: {
            vulkan_hook_shutdown();
        }
        break;
        case GRAPHICS_API_DX12: {
            d3d12_hook_shutdown();
        }
        break;
        default:
            break;
        }
    }

    uninstall_game_hooks();

    if (MH_Uninitialize() != MH_OK)
    {
        LOG_ERROR("Failed to uninitialize MinHook.");
    }
    else
    {
        LOG_DEBUG("MinHook uninitialized.");
    }
    shutdown_logger();
}
