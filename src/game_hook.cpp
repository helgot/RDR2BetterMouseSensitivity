#include "game_hook.hpp"

#include <MinHook.h>
#include <stdint.h>
#include <windows.h>

#include "logger.hpp"
#include "mod_config.hpp"

extern ModConfig Config;

static constexpr float kBaseFirstPersonFov = 55.0f;
static constexpr float kFirstPersonQuakeScale = 1.0f / 1.8227f;
static constexpr float kThirdPersonQuakeScale =
    1.2225f * kFirstPersonQuakeScale;

static constexpr uint64_t kThirdPersonSensitivityFunctionOffset = 0x395E18;
static constexpr uint64_t kFirstPersonSensitivityFunctionOffset = 0x395EE0;
static constexpr uint64_t kThirdPersonHelper1FunctionOffset = 0x391B78;
static constexpr uint64_t kThirdPersonHelper2FunctionOffset = 0x3915A8;
static constexpr uint64_t kThirdPersonFloatOffset = 0x39806BC;
static constexpr uint64_t kFirstPersonFloatOffset = 0x39806FC;
static constexpr uint64_t kFovFloatOffset = 0x3EA0BE0;

static uint64_t ThirdPersonSensitivityFunctionAddr;
static uint64_t FirstPersonSensitivityFunctionAddr;
static uint64_t ThirdPersonHelper1FunctionAddr;
static uint64_t ThirdPersonHelper2FunctionAddr;
static uint64_t ThirdPersonFloatAddr;
static uint64_t FirstPersonFloatAddr;
static uint64_t FovFloatAddr;

typedef __int64(__fastcall *ThirdPersonMouseSensitivityFunctionPtr)(int64_t a1,
                                                                    int64_t a2,
                                                                    float a3,
                                                                    float a4);
typedef void(__fastcall *FirstPersonMouseSensitivityFunctionPtr)(
    int64_t a1, int64_t a2, int64_t a3, float a4, float a5);

typedef void(__fastcall *ThirdPersonHelper1FunctionPtr)(float *a1, float *a2);
typedef __int64(__fastcall *ThirdPersonHelper2FunctionPtr)(__int64 a1);

static ThirdPersonMouseSensitivityFunctionPtr OriginalThirdPersonSensitivity;
static FirstPersonMouseSensitivityFunctionPtr OriginalFirstPersonSensitivity;
static ThirdPersonHelper1FunctionPtr ThirdPersonHelper1;
static ThirdPersonHelper2FunctionPtr ThirdPersonHelper2;

static uint64_t GameBaseAddress = 0;

static __int64 __fastcall ThirdPersonMouseSensitivity(int64_t a1, int64_t a2,
                                                      float a3, float a4);
static void __fastcall FirstPersonMouseSensitivity(__int64 a1, __int64 a2,
                                                   __int64 a3, float a4,
                                                   float a5);

bool InstallGameHooks()
{
    GameBaseAddress = (uint64_t)GetModuleHandleA(NULL);
    if (GameBaseAddress)
    {
        LOG_DEBUG("Got game base address: 0x%llx", GameBaseAddress);
    }
    else
    {
        LOG_DEBUG("Failed to get game base address: 0x%llx", GameBaseAddress);
        return false;
    }

    // Set Addresses for functions and values that we'll be using from the game.
    ThirdPersonSensitivityFunctionAddr =
        GameBaseAddress + kThirdPersonSensitivityFunctionOffset;
    FirstPersonSensitivityFunctionAddr =
        GameBaseAddress + kFirstPersonSensitivityFunctionOffset;
    ThirdPersonHelper1FunctionAddr =
        GameBaseAddress + kThirdPersonHelper1FunctionOffset;
    ThirdPersonHelper2FunctionAddr =
        GameBaseAddress + kThirdPersonHelper2FunctionOffset;
    ThirdPersonFloatAddr = GameBaseAddress + kThirdPersonFloatOffset;
    FirstPersonFloatAddr = GameBaseAddress + kFirstPersonFloatOffset;
    FovFloatAddr = GameBaseAddress + kFovFloatOffset;

    ThirdPersonHelper1 =
        (ThirdPersonHelper1FunctionPtr)(ThirdPersonHelper1FunctionAddr);
    ThirdPersonHelper2 =
        (ThirdPersonHelper2FunctionPtr)(ThirdPersonHelper2FunctionAddr);

    //-------------------------------------------------------------------------
    // Third-Person Sensitivity Hook
    //-------------------------------------------------------------------------
    if (MH_CreateHook((LPVOID)ThirdPersonSensitivityFunctionAddr,
                      (LPVOID)&ThirdPersonMouseSensitivity,
                      (LPVOID *)(&OriginalThirdPersonSensitivity)) != MH_OK)
    {
        LOG_ERROR("Failed to create hook for ThirdPersonMouseSensitivity.");
        return false;
    }

    if (MH_EnableHook((LPVOID)ThirdPersonSensitivityFunctionAddr) != MH_OK)
    {
        LOG_ERROR("Failed to enable hook for ThirdPersonMouseSensitivity.");
        return false;
    }
    LOG_INFO("Hook for ThirdPersonMouseSensitivity created and enabled.");

    //-------------------------------------------------------------------------
    // First-Person Sensitivity Hook
    //-------------------------------------------------------------------------
    if (MH_CreateHook((LPVOID)FirstPersonSensitivityFunctionAddr,
                      (LPVOID)&FirstPersonMouseSensitivity,
                      (LPVOID *)&OriginalFirstPersonSensitivity) != MH_OK)
    {
        LOG_ERROR("Failed to create hook for FirstPersonMouseSensitivity.");
        return false;
    }
    if (MH_EnableHook((LPVOID)FirstPersonSensitivityFunctionAddr) != MH_OK)
    {
        LOG_ERROR("Failed to enable hook for FirstPersonMouseSensitivity.");
        return false;
    }
    LOG_INFO("Hook for FirstPersonMouseSensitivity created and enabled.");

    return true;
}

void UninstallGameHooks()
{
    MH_DisableHook((LPVOID)ThirdPersonSensitivityFunctionAddr);
    MH_RemoveHook((LPVOID)ThirdPersonSensitivityFunctionAddr);
    MH_DisableHook((LPVOID)FirstPersonSensitivityFunctionAddr);
    MH_RemoveHook((LPVOID)FirstPersonSensitivityFunctionAddr);
}

static __int64 __fastcall ThirdPersonMouseSensitivity(int64_t a1, int64_t a2,
                                                      float a3, float a4)
{
    // The in-menu game slider will now have no contribution.
    float InGameSliderContribution = 0.0f;
    float *v6 = *(float **)(a1 + 40);

    float aiming = *(float *)(a1 + 48); // 1.0f if aimed, 0.0f if not.
                                        // (interpolated in transition)
    float AimingScale = 1.0f + aiming * (Config.ThirdPersonADSScale - 1.0f);
    float ScaleProduct =
        Config.ThirdPersonSensitivity * AimingScale * kThirdPersonQuakeScale;

    float ScaleX, ScaleY;
    ScaleX = (float)((float)((float)((float)(v6[12] - v6[11]) *
                                     InGameSliderContribution) +
                             v6[11]) *
                     0.017453292f) *
             *(float *)ThirdPersonFloatAddr * ScaleProduct;
    ScaleY = (float)((float)((float)((float)(v6[14] - v6[13]) *
                                     InGameSliderContribution) +
                             v6[13]) *
                     0.017453292f) *
             *(float *)ThirdPersonFloatAddr * ScaleProduct;

    ThirdPersonHelper1(&ScaleX, &ScaleY);

    *(float *)(a1 + 88) = ScaleX * a3;
    *(float *)(a1 + 92) = ScaleY * a4;

    return ThirdPersonHelper2(a1);
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
    float AimingScale =
        1.0f + (is_aiming ? 1.0f : 0.0f) * (Config.FirstPersonADSScale - 1.0f);

    float Fov = *(float *)FovFloatAddr;

    // Note(harrison): By default the game scales aim sensivity with FOV.
    float FovScale = 1.0f;
    if (!Config.FirstPersonFovSensitivityScaling)
    {
        FovScale = Fov / kBaseFirstPersonFov;
    }
    float ScaleProduct = Config.FirstPersonADSScale * AimingScale *
                         kFirstPersonQuakeScale / FovScale;

    // Fix scaling to be consistent across non-scoped and scoped modes.
    *(float *)(a1 + 84) = 190.0f;
    *(float *)(a1 + 76) = 250.0f;

    float SensX = (float)((float)((float)((float)(*(float *)(a1 + 80) -
                                                  *(float *)(a1 + 76)) *
                                          v9) +
                                  *(float *)(a1 + 76)) *
                          0.017453292f) *
                  *(float *)FirstPersonFloatAddr * ScaleProduct;
    float SensY = (float)((float)((float)((float)(*(float *)(a1 + 88) -
                                                  *(float *)(a1 + 84)) *
                                          v9) +
                                  *(float *)(a1 + 84)) *
                          0.017453292f) *
                  *(float *)FirstPersonFloatAddr * ScaleProduct;

    *(float *)(a2 + 128) = (a4 * *(float *)(a1 + 240)) * SensX;
    // Clamp vertical rotation.
    float FinalY = (a5 * *(float *)(a1 + 244)) * SensY;
    *(float *)(a2 + 136) = fminf(3.1415927f, fmaxf(-3.1415927f, FinalY));
}