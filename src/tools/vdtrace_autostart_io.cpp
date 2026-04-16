#include "pch.h"
#include "tools/vdtrace_autostart_internal.h"

namespace vdtrace::tools::autostart_cli
{
    std::string NarrowUtf8(const std::wstring &text)
    {
        if (text.empty())
        {
            return {};
        }

        const int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (count <= 1)
        {
            return {};
        }

        std::string result(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), count, nullptr, nullptr);
        if (!result.empty() && result.back() == '\0')
        {
            result.pop_back();
        }
        return result;
    }

    void WriteText(HANDLE handle, const std::wstring &text)
    {
        if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        {
            return;
        }

        DWORD console_mode = 0;
        if (GetConsoleMode(handle, &console_mode) != FALSE)
        {
            DWORD written = 0;
            WriteConsoleW(handle, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
            return;
        }

        const std::string utf8 = NarrowUtf8(text);
        DWORD written = 0;
        WriteFile(handle, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    }

    void PrintOut(const std::wstring &text)
    {
        WriteText(GetStdHandle(STD_OUTPUT_HANDLE), text + L"\n");
    }

    void PrintErr(const std::wstring &text)
    {
        WriteText(GetStdHandle(STD_ERROR_HANDLE), text + L"\n");
    }

    void PrintUsage()
    {
        PrintOut(L"用法:");
        PrintOut(L"  vdtrace_autostart.exe [config_path]");
        PrintOut(L"  vdtrace_autostart.exe --restore [config_path]");
    }

    std::wstring BuildTimestampSuffix()
    {
        SYSTEMTIME now = {};
        GetLocalTime(&now);
        wchar_t buffer[64] = {};
        swprintf_s(
            buffer,
            L"%04u%02u%02u-%02u%02u%02u",
            static_cast<unsigned>(now.wYear),
            static_cast<unsigned>(now.wMonth),
            static_cast<unsigned>(now.wDay),
            static_cast<unsigned>(now.wHour),
            static_cast<unsigned>(now.wMinute),
            static_cast<unsigned>(now.wSecond));
        return buffer;
    }

    bool ParseStatusField(const std::wstring &text, const std::wstring &key, uint64_t &value)
    {
        const size_t begin = text.find(key);
        if (begin == std::wstring::npos)
        {
            return false;
        }

        size_t cursor = begin + key.size();
        value = 0;
        bool seen_digit = false;
        while (cursor < text.size() && text[cursor] >= L'0' && text[cursor] <= L'9')
        {
            seen_digit = true;
            value = value * 10u + static_cast<uint64_t>(text[cursor] - L'0');
            ++cursor;
        }
        return seen_digit;
    }

    std::wstring BuildCommandLine(const std::wstring &game_path, const std::wstring &arguments)
    {
        if (arguments.empty())
        {
            return L"\"" + game_path + L"\"";
        }

        return L"\"" + game_path + L"\" " + arguments;
    }

    std::filesystem::path BuildHelperLogPath(const std::filesystem::path &config_path)
    {
        const auto base_directory = config_path.parent_path().empty() ? std::filesystem::current_path() : config_path.parent_path();
        std::error_code ec;
        std::filesystem::create_directories(base_directory / L"traces", ec);
        return (base_directory / L"traces" / (L"VDTraceAutoStart-" + BuildTimestampSuffix() + L".log")).lexically_normal();
    }

    bool IsProcessAlive(HANDLE process)
    {
        DWORD exit_code = STILL_ACTIVE;
        return GetExitCodeProcess(process, &exit_code) != FALSE && exit_code == STILL_ACTIVE;
    }

    bool TryGetProcessExitCodeValue(HANDLE process, DWORD &exit_code)
    {
        exit_code = STILL_ACTIVE;
        return process != nullptr
            && process != INVALID_HANDLE_VALUE
            && GetExitCodeProcess(process, &exit_code) != FALSE
            && exit_code != STILL_ACTIVE;
    }

    std::wstring FormatProcessExitCodeText(HANDLE process)
    {
        DWORD exit_code = STILL_ACTIVE;
        if (!TryGetProcessExitCodeValue(process, exit_code))
        {
            return L"exit=unknown";
        }

        wchar_t buffer[64] = {};
        swprintf_s(buffer, L"exit=%lu (0x%08lX)", static_cast<unsigned long>(exit_code), static_cast<unsigned long>(exit_code));
        return buffer;
    }
}
