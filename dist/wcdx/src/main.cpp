#include "platform.h"
#include "wcdx_ini.h"

HINSTANCE DllInstance;

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, [[maybe_unused]] LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        DllInstance = hInstDLL;
        wcdx_ini_Load("wcdx.ini");
        break;

    case DLL_PROCESS_DETACH:
        wcdx_ini_Save("wcdx.ini");
        break;
   }

    return TRUE;
}
