#include "pch.h"
#include "loader/VDTraceWinHttpProxy.hpp"

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    return VDTraceLoader::HandleDllMain(instance, reason, reserved);
}
