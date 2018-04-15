#include "platform.h"

HINSTANCE DllInstance;

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        DllInstance = hInstDLL;
        break;
    }

    return TRUE;
}