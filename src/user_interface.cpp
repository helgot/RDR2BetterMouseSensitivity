#include "user_interface.hpp"

#include <imgui.h>
#include <windows.h>

#include "mod_config.hpp"
#include "version.h"

extern ModConfig CONFIG;

bool MENU_VISIBLE;

static void draw_config_helper();
static void draw_config_io_helper();

void render_ui()
{
    if (!MENU_VISIBLE)
        return;

    char Title[100];
    sprintf(Title,
            "RDR2BetterMouseSensitivity by Helgot, Version: %s (Toggle: F10)",
            APP_GIT_DESCRIBE);
    ImGui::Begin(Title);
    draw_config_helper();
    draw_config_io_helper();
    ImGui::End();
}

void handle_input()
{
    static bool wasF10Pressed = false;
    bool isF10Pressed = GetAsyncKeyState(VK_F10) & 0x8000;

    if (isF10Pressed && !wasF10Pressed)
    {
        MENU_VISIBLE = !MENU_VISIBLE;

        // Show/hide the mouse cursor along with the menu
        ImGuiIO &io = ImGui::GetIO();
        io.MouseDrawCursor = MENU_VISIBLE;
    }
    wasF10Pressed = isF10Pressed;
}

static void draw_config_helper()
{
    constexpr float MinSliderValue = 0.0f;
    constexpr float MaxSilderValue = 10.0f;
    ImGui::Checkbox("First-Person FOV Scaling",
                    &CONFIG.first_person_fov_sensitivity_scaling);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Scale the first-person sensitivity with FOV."
                    "By default the game scales the senvitivity with fov in "
                    "first-person.");
        ImGui::EndTooltip();
    }

    ImGui::SliderFloat("First-Person Sensitivity",
                       &CONFIG.first_person_sensitivity_scale, MinSliderValue,
                       MaxSilderValue);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(
            "First-person sensitivity (overrides the in-game setting).");
        ImGui::EndTooltip();
    }

    ImGui::SliderFloat("Third-Person Sensitivity",
                       &CONFIG.third_person_sensitivity_scale, MinSliderValue,
                       MaxSilderValue);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(
            "Third-person sensitivity (overrides the in-game setting).");
        ImGui::EndTooltip();
    }

    ImGui::SliderFloat("First-Person ADS Scale", &CONFIG.first_person_ads_scale,
                       MinSliderValue, MaxSilderValue);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("First-person ADS scale multiplier (overrides the in-game "
                    "setting).");
        ImGui::EndTooltip();
    }

    ImGui::SliderFloat("Third-Person ADS scale", &CONFIG.third_person_ads_scale,
                       MinSliderValue, MaxSilderValue);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Third-person ADS scale mulitplier (overrides the in-game "
                    "setting).");
        ImGui::EndTooltip();
    }

    ImGui::Checkbox("Show Menu on Start-up", &CONFIG.show_menu_at_start_up);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("Determines whether the menu will be shown at the start-up "
                    "of the game.");
        ImGui::EndTooltip();
    }

    ImGui::Checkbox("User Interface Enabled (Requires Restart)",
                    &CONFIG.user_interface_enabled);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text(
            "Determines whether mod will hook D3D12/Vulkan and display "
            "this interface [false == .ini only mode]. (Requires Restart.)");
        ImGui::EndTooltip();
    }
}

static void draw_config_io_helper()
{
    ImGui::Separator();
    if (ImGui::Button("Save Config"))
    {
        // Update the main config and save.
        save_config(&CONFIG);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Config"))
    {
        // Reload from disk into the manager, then update our editable copy.
        CONFIG = load_config();
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Defaults"))
    {
        // Create a default config and update our editable copy.
        CONFIG = default_config();
    }
    ImGui::Separator();
}