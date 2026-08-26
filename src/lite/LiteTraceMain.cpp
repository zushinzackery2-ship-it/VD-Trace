#include "pch.h"
#include "lite/LiteTraceRuntime.h"

namespace
{
    std::atomic<bool> g_lite_started = false;

    bool StartLiteTraceThread()
    {
        bool expected = false;
        if (!g_lite_started.compare_exchange_strong(expected, true))
        {
            return true;
        }

        const HANDLE thread = CreateThread(nullptr, 0, vdtrace::lite::LiteTraceThreadMain, nullptr, 0, nullptr);
        if (thread == nullptr)
        {
            g_lite_started.store(false);
            return false;
        }

        CloseHandle(thread);
        return true;
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    (void) reserved;
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(module);
        StartLiteTraceThread();
    }
    return TRUE;
}

extern "C"
{
    __declspec(dllexport) BOOL vdtrace_lite_bootstrap(void)
    {
        return StartLiteTraceThread() ? TRUE : FALSE;
    }
}
