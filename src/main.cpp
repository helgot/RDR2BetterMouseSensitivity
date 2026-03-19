#include <windows.h>

#include "mod_main.hpp"

DWORD WINAPI intialize_mod_thread(LPVOID module)
{
    init_mod();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(module);
        CreateThread(NULL, 0, intialize_mod_thread, NULL, 0, NULL);
        OutputDebugStringA("RDR2MouseSensitivity.asi attach.\n");
    }
    break;
    case DLL_PROCESS_DETACH: {
        OutputDebugStringA("RDR2MouseSensitivity.asi detach.\n");
        shutdown_mod();
    }
    break;
    default: {
    }
    }
    return true;
}
