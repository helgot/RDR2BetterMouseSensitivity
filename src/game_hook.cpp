#include "game_hook.hpp"

#include <MinHook.h>
#include <stdint.h>
#include <windows.h>

#include "logger.hpp"
#include "mod_config.hpp"

extern ModConfig CONFIG;

static constexpr float BASE_FIRST_PERSON_FOV = 55.0f;
static constexpr float FIRST_PERSON_QUAKE_SCALE = 1.0f / 1.8227f;
static constexpr float THIRD_PERSON_QUAKE_SCALE =
    1.2225f * FIRST_PERSON_QUAKE_SCALE;

static constexpr uint64_t THIRD_PERSON_SENSITIVITY_FUNCTION_OFFSET = 0x395E18;
static constexpr uint64_t FIRST_PERSON_SENSITIVITY_FUNCTION_OFFSET = 0x395EE0;
static constexpr uint64_t THIRD_PERSON_HELPER_1_FUNCTION_OFFSET = 0x391B78;
static constexpr uint64_t THIRD_PERSON_HELPER_2_FUNCTION_OFFSET = 0x3915A8;
static constexpr uint64_t THIRD_PERSON_FLOAT_OFFSET = 0x39806BC;
static constexpr uint64_t FIRST_PERSON_FLOAT_OFFSET = 0x39806FC;
static constexpr uint64_t FOV_FLOAT_OFFSET = 0x3EA0BE0;

static uint64_t THIRD_PERSON_SENSITIVY_FUNCTION_ADDR;
static uint64_t FIRST_PERSON_SENSITIVITY_FUNCTION_ADDR;
static uint64_t THIRD_PERSON_HELPER_1_FUNCTION_ADDR;
static uint64_t THIRD_PERSON_HELPER_2_FUNCTION_ADDR;
static uint64_t THIRD_PERSON_FLOAT_SCALE_ADDR;
static uint64_t FIRST_PERSON_FLOAT_SCALE_ADDR;
static uint64_t FOV_FLOAT_ADDR;

typedef __int64(__fastcall *ThirdPersonMouseSensitivityFunctionPtr)(int64_t a1,
                                                                    int64_t a2,
                                                                    float a3,
                                                                    float a4);
typedef void(__fastcall *FirstPersonMouseSensitivityFunctionPtr)(
    int64_t a1, int64_t a2, int64_t a3, float a4, float a5);

typedef void(__fastcall *ThirdPersonHelper1FunctionPtr)(float *a1, float *a2);
typedef __int64(__fastcall *ThirdPersonHelper2FunctionPtr)(__int64 a1);

static ThirdPersonMouseSensitivityFunctionPtr original_third_person_sensitivity;
static FirstPersonMouseSensitivityFunctionPtr original_first_person_sensitivity;
static ThirdPersonHelper1FunctionPtr third_person_helper_1;
static ThirdPersonHelper2FunctionPtr third_person_helper_2;

static uint64_t game_base_addr = 0;

static __int64 __fastcall ThirdPersonMouseSensitivity(int64_t a1, int64_t a2,
                                                      float a3, float a4);
static void __fastcall FirstPersonMouseSensitivity(__int64 a1, __int64 a2,
                                                   __int64 a3, float a4,
                                                   float a5);

bool install_game_hooks()
{
    game_base_addr = (uint64_t)GetModuleHandleA(NULL);
    if (game_base_addr)
    {
        LOG_DEBUG("Got game base address: 0x%llx", game_base_addr);
    }
    else
    {
        LOG_DEBUG("Failed to get game base address: 0x%llx", game_base_addr);
        return false;
    }

    // Set Addresses for functions and values that we'll be using from the game.
    THIRD_PERSON_SENSITIVY_FUNCTION_ADDR =
        game_base_addr + THIRD_PERSON_SENSITIVITY_FUNCTION_OFFSET;
    FIRST_PERSON_SENSITIVITY_FUNCTION_ADDR =
        game_base_addr + FIRST_PERSON_SENSITIVITY_FUNCTION_OFFSET;
    THIRD_PERSON_HELPER_1_FUNCTION_ADDR =
        game_base_addr + THIRD_PERSON_HELPER_1_FUNCTION_OFFSET;
    THIRD_PERSON_HELPER_2_FUNCTION_ADDR =
        game_base_addr + THIRD_PERSON_HELPER_2_FUNCTION_OFFSET;
    THIRD_PERSON_FLOAT_SCALE_ADDR = game_base_addr + THIRD_PERSON_FLOAT_OFFSET;
    FIRST_PERSON_FLOAT_SCALE_ADDR = game_base_addr + FIRST_PERSON_FLOAT_OFFSET;
    FOV_FLOAT_ADDR = game_base_addr + FOV_FLOAT_OFFSET;

    third_person_helper_1 =
        (ThirdPersonHelper1FunctionPtr)(THIRD_PERSON_HELPER_1_FUNCTION_ADDR);
    third_person_helper_2 =
        (ThirdPersonHelper2FunctionPtr)(THIRD_PERSON_HELPER_2_FUNCTION_ADDR);

    //-------------------------------------------------------------------------
    // Third-Person Sensitivity Hook
    //-------------------------------------------------------------------------
    if (MH_CreateHook((LPVOID)THIRD_PERSON_SENSITIVY_FUNCTION_ADDR,
                      (LPVOID)&ThirdPersonMouseSensitivity,
                      (LPVOID *)(&original_third_person_sensitivity)) != MH_OK)
    {
        LOG_ERROR("Failed to create hook for ThirdPersonMouseSensitivity.");
        return false;
    }

    if (MH_EnableHook((LPVOID)THIRD_PERSON_SENSITIVY_FUNCTION_ADDR) != MH_OK)
    {
        LOG_ERROR("Failed to enable hook for ThirdPersonMouseSensitivity.");
        return false;
    }
    LOG_INFO("Hook for ThirdPersonMouseSensitivity created and enabled.");

    //-------------------------------------------------------------------------
    // First-Person Sensitivity Hook
    //-------------------------------------------------------------------------
    if (MH_CreateHook((LPVOID)FIRST_PERSON_SENSITIVITY_FUNCTION_ADDR,
                      (LPVOID)&FirstPersonMouseSensitivity,
                      (LPVOID *)&original_first_person_sensitivity) != MH_OK)
    {
        LOG_ERROR("Failed to create hook for FirstPersonMouseSensitivity.");
        return false;
    }
    if (MH_EnableHook((LPVOID)FIRST_PERSON_SENSITIVITY_FUNCTION_ADDR) != MH_OK)
    {
        LOG_ERROR("Failed to enable hook for FirstPersonMouseSensitivity.");
        return false;
    }
    LOG_INFO("Hook for FirstPersonMouseSensitivity created and enabled.");

    return true;
}

void uninstall_game_hooks()
{
    MH_DisableHook((LPVOID)THIRD_PERSON_SENSITIVY_FUNCTION_ADDR);
    MH_RemoveHook((LPVOID)THIRD_PERSON_SENSITIVY_FUNCTION_ADDR);
    MH_DisableHook((LPVOID)FIRST_PERSON_SENSITIVITY_FUNCTION_ADDR);
    MH_RemoveHook((LPVOID)FIRST_PERSON_SENSITIVITY_FUNCTION_ADDR);
}

static __int64 __fastcall ThirdPersonMouseSensitivity(int64_t a1, int64_t a2,
                                                      float a3, float a4)
{
    // The in-menu game slider will now have no contribution.
    float in_game_slider_contribution = 0.0f;
    float *v6 = *(float **)(a1 + 40);

    float aiming_multiplier =
        *(float *)(a1 + 48); // 1.0f if aimed, 0.0f if not.
                             // (interpolated in transition)
    float aiming_scale =
        1.0f + aiming_multiplier * (CONFIG.third_person_ads_scale - 1.0f);
    float scale_product = CONFIG.third_person_sensitivity_scale * aiming_scale *
                          THIRD_PERSON_QUAKE_SCALE;

    float scale_x, scale_y;
    scale_x = (float)((float)((float)((float)(v6[12] - v6[11]) *
                                      in_game_slider_contribution) +
                              v6[11]) *
                      0.017453292f) *
              *(float *)THIRD_PERSON_FLOAT_SCALE_ADDR * scale_product;
    scale_y = (float)((float)((float)((float)(v6[14] - v6[13]) *
                                      in_game_slider_contribution) +
                              v6[13]) *
                      0.017453292f) *
              *(float *)THIRD_PERSON_FLOAT_SCALE_ADDR * scale_product;

    third_person_helper_1(&scale_x, &scale_y);

    *(float *)(a1 + 88) = scale_x * a3;
    *(float *)(a1 + 92) = scale_y * a4;

    return third_person_helper_2(a1);
}

static void __fastcall FirstPersonMouseSensitivity(__int64 a1, __int64 a2,
                                                   __int64 a3, float a4,
                                                   float a5)
{
    int v7;
    if (*(char *)(a2 + 208) < 0)
        v7 = 0;
    else
        v7 = 1065353216;

    *(DWORD *)(a1 + 228) = v7;
    *(float *)(a1 + 168) = a4;
    *(float *)(a1 + 172) = a5;

    // Zero out the in-game slider contribution.
    float v9 = 0.0f;

    bool is_aiming = (*(BYTE *)(a2 + 211) & 8) == 0x8;
    float aiming_scale = 1.0f + (is_aiming ? 1.0f : 0.0f) *
                                    (CONFIG.first_person_ads_scale - 1.0f);

    float current_fov = *(float *)FOV_FLOAT_ADDR;

    // Note(harrison): By default the game scales aim sensivity with FOV.
    float fov_scale = 1.0f;
    if (!CONFIG.first_person_fov_sensitivity_scaling)
    {
        fov_scale = current_fov / BASE_FIRST_PERSON_FOV;
    }
    float scale_product = CONFIG.first_person_ads_scale * aiming_scale *
                          FIRST_PERSON_QUAKE_SCALE / fov_scale;

    // Fix scaling to be consistent across non-scoped and scoped modes.
    *(float *)(a1 + 84) = 190.0f;
    *(float *)(a1 + 76) = 250.0f;

    float sensitivity_x = (float)((float)((float)((float)(*(float *)(a1 + 80) -
                                                          *(float *)(a1 + 76)) *
                                                  v9) +
                                          *(float *)(a1 + 76)) *
                                  0.017453292f) *
                          *(float *)FIRST_PERSON_FLOAT_SCALE_ADDR *
                          scale_product;
    float sensitivity_y = (float)((float)((float)((float)(*(float *)(a1 + 88) -
                                                          *(float *)(a1 + 84)) *
                                                  v9) +
                                          *(float *)(a1 + 84)) *
                                  0.017453292f) *
                          *(float *)FIRST_PERSON_FLOAT_SCALE_ADDR *
                          scale_product;

    *(float *)(a2 + 128) = (a4 * *(float *)(a1 + 240)) * sensitivity_x;
    // Clamp vertical rotation.
    float output_y = (a5 * *(float *)(a1 + 244)) * sensitivity_y;
    *(float *)(a2 + 136) = fminf(3.1415927f, fmaxf(-3.1415927f, output_y));
}