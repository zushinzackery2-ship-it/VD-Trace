#include "pch.h"

#include <filesystem>

#pragma comment(linker, "/export:Forward1=EndfieldBase_original.#1,@1,NONAME")

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    std::wstring ReadActivationValue(const std::filesystem::path &path, const wchar_t *key)
    {
        wchar_t buffer[1024] = {};
        const DWORD length = GetPrivateProfileStringW(L"autostart", key, L"", buffer, static_cast<DWORD>(std::size(buffer)), path.c_str());
        return length == 0 ? std::wstring() : std::wstring(buffer, buffer + length);
    }

    DWORD WINAPI BootstrapThreadMain(void *)
    {
        std::wstring module_path(MAX_PATH, L'\0');
        DWORD module_length = GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), module_path.data(), static_cast<DWORD>(module_path.size()));
        while (module_length == module_path.size())
        {
            module_path.resize(module_path.size() * 2);
            module_length = GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase), module_path.data(), static_cast<DWORD>(module_path.size()));
        }
        if (module_length == 0)
        {
            return 0;
        }
        module_path.resize(module_length);

        const std::filesystem::path activation_path = std::filesystem::path(module_path).parent_path() / L"VDTraceAutoStart.activate.ini";
        const std::wstring helper_path = ReadActivationValue(activation_path, L"helper_path");
        if (helper_path.empty())
        {
            return 0;
        }

        const std::wstring config_path = ReadActivationValue(activation_path, L"config_path");
        const std::wstring log_path = ReadActivationValue(activation_path, L"log_path");
        if (!config_path.empty())
        {
            SetEnvironmentVariableW(L"VDTRACE_AUTOSTART_CONFIG", config_path.c_str());
        }
        if (!log_path.empty())
        {
            SetEnvironmentVariableW(L"VDTRACE_AUTOSTART_LOG", log_path.c_str());
        }
        SetEnvironmentVariableW(L"VDTRACE_AUTOSTART_HELPER", helper_path.c_str());

        const HMODULE helper = LoadLibraryW(helper_path.c_str());
        if (helper == nullptr)
        {
            return 0;
        }

        using BootstrapFn = BOOL(WINAPI *)();
        const auto bootstrap = reinterpret_cast<BootstrapFn>(GetProcAddress(helper, "vdtrace_loader_bootstrap"));
        if (bootstrap == nullptr)
        {
            return 0;
        }

        bootstrap();
        return 0;
    }
}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void) reserved;

    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        const HANDLE thread = CreateThread(nullptr, 0, BootstrapThreadMain, nullptr, 0, nullptr);
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
    }

    return TRUE;
}
