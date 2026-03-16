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

ModConfig Config;
extern bool MenuVisible;

static GraphicsAPI ConfiguredAPI = GRAPHICS_API_UNKNOWN;

void InitMod()
{
    InitLogger(LOG_LEVEL_DEBUG);
    Config = LoadConfig();
    SaveConfig(&Config);

    MenuVisible = Config.ShowMenuAtStartUp;

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

    InstallGameHooks();

    if (Config.UserInterfaceEnabled)
    {
        ConfiguredAPI = GetGraphicsApiFromSettingsFile();
        switch (ConfiguredAPI)
        {
        case GRAPHICS_API_VULKAN: {
            VulkanHookInstall();
        }
        break;
        case GRAPHICS_API_DX12: {
            D3D12HookInstall();
        }
        break;
        default:
            break;
        }
    }
}

void ShutdownMod()
{

    if (Config.UserInterfaceEnabled)
    {
        switch (ConfiguredAPI)
        {
        case GRAPHICS_API_VULKAN: {
            VulkanHookShutdown();
        }
        break;
        case GRAPHICS_API_DX12: {
            D3D12HookShutdown();
        }
        break;
        default:
            break;
        }
    }

    UninstallGameHooks();

    if (MH_Uninitialize() != MH_OK)
    {
        LOG_ERROR("Failed to uninitialize MinHook.");
    }
    else
    {
        LOG_DEBUG("MinHook uninitialized.");
    }
    ShutdownLogger();
}
