#include "pch.h"
#include "core/threading/VDTraceThreadContextInternal.h"

namespace vdtrace::thread_context_detail
{
    std::wstring GetModuleFilePath(HMODULE module)
    {
        if (module == nullptr)
        {
            return {};
        }

        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        while (length == buffer.size())
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        }

        if (length == 0)
        {
            return {};
        }

        buffer.resize(length);
        return buffer;
    }

    std::wstring ToLowerCopy(const std::wstring &text)
    {
        std::wstring result = text;
        std::transform(
            result.begin(),
            result.end(),
            result.begin(),
            [](wchar_t ch)
            {
                return static_cast<wchar_t>(towlower(ch));
            });
        return result;
    }

    bool StartsWith(const std::wstring &text, const std::wstring &prefix)
    {
        return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
    }

    bool IsKnownSystemModuleName(const std::wstring &name)
    {
        static const std::unordered_set<std::wstring> names = {
            L"ntdll.dll", L"kernel32.dll", L"kernelbase.dll", L"user32.dll", L"gdi32.dll",
            L"gdi32full.dll", L"advapi32.dll", L"ucrtbase.dll", L"msvcrt.dll", L"vcruntime140.dll",
            L"vcruntime140_1.dll", L"msvcp140.dll", L"rpcrt4.dll", L"combase.dll", L"comdlg32.dll",
            L"ole32.dll", L"oleaut32.dll", L"shell32.dll", L"shlwapi.dll", L"bcrypt.dll",
            L"bcryptprimitives.dll", L"crypt32.dll", L"ws2_32.dll", L"winmm.dll", L"winhttp.dll",
            L"wininet.dll", L"sechost.dll", L"imm32.dll", L"version.dll", L"cfgmgr32.dll",
            L"setupapi.dll", L"api-ms-win-core-synch-l1-2-0.dll",
            L"api-ms-win-core-libraryloader-l1-2-0.dll",
            L"api-ms-win-core-processthreads-l1-1-0.dll",
        };
        return names.find(name) != names.end();
    }

    bool IsSystemModulePath(const std::wstring &path)
    {
        if (path.empty())
        {
            return false;
        }

        const std::wstring normalized = ToLowerCopy(std::filesystem::path(path).lexically_normal().wstring());
        const std::wstring filename = ToLowerCopy(std::filesystem::path(normalized).filename().wstring());
        if (IsKnownSystemModuleName(filename))
        {
            return true;
        }

        wchar_t windows_dir_buffer[MAX_PATH] = {};
        const UINT windows_dir_length = GetWindowsDirectoryW(windows_dir_buffer, MAX_PATH);
        if (windows_dir_length == 0 || windows_dir_length >= MAX_PATH)
        {
            return false;
        }

        std::wstring windows_dir = ToLowerCopy(std::filesystem::path(windows_dir_buffer).lexically_normal().wstring());
        if (!windows_dir.empty() && windows_dir.back() != L'\\')
        {
            windows_dir += L'\\';
        }

        return StartsWith(normalized, windows_dir + L"system32\\")
            || StartsWith(normalized, windows_dir + L"syswow64\\")
            || StartsWith(normalized, windows_dir + L"winsxs\\");
    }

    bool TryReadPointerValue(uintptr_t pointer_value, uintptr_t &value)
    {
        value = 0;
        if (pointer_value == 0)
        {
            return false;
        }

        __try
        {
            value = *reinterpret_cast<uintptr_t *>(pointer_value);
            return value != 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            value = 0;
            return false;
        }
    }

    void ApplyExecutionControls(CONTEXT &context, const uintptr_t *addresses, uint32_t count, bool single_step)
    {
        context.Dr0 = count > 0 ? addresses[0] : 0;
        context.Dr1 = count > 1 ? addresses[1] : 0;
        context.Dr2 = count > 2 ? addresses[2] : 0;
        context.Dr3 = count > 3 ? addresses[3] : 0;
        context.Dr6 = 0;
        context.Dr7 = 0;
        for (uint32_t index = 0; index < count && index < 4; index++)
        {
            context.Dr7 |= (static_cast<DWORD_PTR>(1) << (index * 2));
        }

        if (single_step)
        {
            context.EFlags |= 0x100u;
        }
        else
        {
            context.EFlags &= ~0x100u;
        }
    }
}
