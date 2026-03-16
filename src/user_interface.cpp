#include "user_interface.hpp"

#include <imgui.h>
#include <windows.h>

#include "mod_config.hpp"
#include "version.h"

extern ModConfig Config;

bool MenuVisible;

static void DrawControlConfigHelper();
static void DrawConfigIOHelper();

void RenderUI()
{
    if (!MenuVisible)
        return;

    char Title[100];
    sprintf(Title,
            "RDR2BetterMouseSensitivity by Helgot, Version: %s (Toggle: F10)",
            APP_GIT_DESCRIBE);
    ImGui::Begin(Title);
    DrawControlConfigHelper();
    DrawConfigIOHelper();
    ImGui::End();
}

void HandleInput()
{
    static bool wasF10Pressed = false;
    bool isF10Pressed = GetAsyncKeyState(VK_F10) & 0x8000;

    if (isF10Pressed && !wasF10Pressed)
    {
        MenuVisible = !MenuVisible;

        // Show/hide the mouse cursor along with the menu
        ImGuiIO &io = ImGui::GetIO();
        io.MouseDrawCursor = MenuVisible;
    }
    wasF10Pressed = isF10Pressed;
}

static void DrawControlConfigHelper()
{
    constexpr float MinSliderValue = 0.0f;
    constexpr float MaxSilderValue = 10.0f;
    ImGui::Checkbox("First-Person FOV Scaling",
                    &Config.FirstPersonFovSensitivityScaling);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Scale the first-person sensitivity with FOV."
                    "Default is true.");
        ImGui::EndTooltip();
    }

    ImGui::SliderFloat("First-Person Sensitivity",
                       &Config.FirstPersonSensitivity, MinSliderValue,
                       MaxSilderValue);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(
            "First-person sensitivity (overrides the in-game setting).");
        ImGui::EndTooltip();
    }

    ImGui::SliderFloat("Third-Person Sensitivity",
                       &Config.ThirdPersonSensitivity, MinSliderValue,
                       MaxSilderValue);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(
            "Third-person sensitivity (overrides the in-game setting).");
        ImGui::EndTooltip();
    }

    ImGui::SliderFloat("First-Person ADS Scale", &Config.FirstPersonADSScale,
                       MinSliderValue, MaxSilderValue);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("First-person ADS scale multiplier (overrides the in-game "
                    "setting).");
        ImGui::EndTooltip();
    }

    ImGui::SliderFloat("Third-Person ADS scale", &Config.ThirdPersonADSScale,
                       MinSliderValue, MaxSilderValue);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Third-person ADS scale mulitplier (overrides the in-game "
                    "setting).");
        ImGui::EndTooltip();
    }

    ImGui::Checkbox("Show Menu on Start-up", &Config.ShowMenuAtStartUp);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Determines whether the menu will be shown at the start-up "
                    "of the game.");
        ImGui::EndTooltip();
    }

    ImGui::Checkbox("User Interface Enabled (Requires Restart)",
                    &Config.UserInterfaceEnabled);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(
            "Determines whether mod will hook D3D12/Vulkan and display "
            "this interface [false == .ini only mode]. (Requires Restart.)");
        ImGui::EndTooltip();
    }
}

static void DrawConfigIOHelper()
{
    ImGui::Separator();
    if (ImGui::Button("Save Config"))
    {
        // Update the main config and save.
        SaveConfig(&Config);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Config"))
    {
        // Reload from disk into the manager, then update our editable copy.
        Config = LoadConfig();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Defaults"))
    {
        // Create a default config and update our editable copy.
        Config = DefaultConfig();
    }
    ImGui::Separator();
}