#include <windows.h>

#include "mod_main.hpp"

DWORD WINAPI InitializeModThread(LPVOID Module)
{
    InitMod();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE Module, DWORD Reason, LPVOID Reserved)
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(Module);
        CreateThread(NULL, 0, InitializeModThread, NULL, 0, NULL);
        OutputDebugStringA("RDR2MouseSensitivity.asi attach.\n");
    }
    break;
    case DLL_PROCESS_DETACH: {
        OutputDebugStringA("RDR2MouseSensitivity.asi detach.\n");
        ShutdownMod();
    }
    break;
    default: {
    }
    }
    return true;
}
