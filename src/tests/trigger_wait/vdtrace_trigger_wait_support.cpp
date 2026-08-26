#include "tests/trigger_wait/vdtrace_trigger_wait_support.h"

#include <Windows.h>

#include <fstream>

namespace trigger_wait_test
{
    std::wstring GetExecutablePath()
    {
        std::wstring buffer(MAX_PATH, L'\0');
        DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        while (length == buffer.size())
        {
            buffer.resize(buffer.size() * 2);
            length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        }

        buffer.resize(length);
        return buffer;
    }

    std::filesystem::path GetExecutableDirectory()
    {
        return std::filesystem::path(GetExecutablePath()).parent_path();
    }

    std::wstring BuildLogPath()
    {
        return (GetExecutableDirectory() / L"VDTraceTriggerWaitTest.log").wstring();
    }

    std::wstring BuildHelperPath()
    {
        return (GetExecutableDirectory() / L"VDTraceTriggerWaitHelper.dll").wstring();
    }

    std::wstring GetFilenameOnly(const std::wstring &path)
    {
        return std::filesystem::path(path).filename().wstring();
    }

    std::wstring ReadFirstMatchingEventLine(const std::wstring &path, const std::wstring &pattern)
    {
        std::ifstream input(std::filesystem::path(path), std::ios::binary);
        if (!input)
        {
            return {};
        }

        std::string line;
        while (std::getline(input, line))
        {
            if (line.rfind("[tid=", 0) == 0)
            {
                const int count = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, nullptr, 0);
                if (count <= 1)
                {
                    return {};
                }

                std::wstring result(static_cast<size_t>(count), L'\0');
                MultiByteToWideChar(CP_UTF8, 0, line.c_str(), -1, result.data(), count);
                if (!result.empty() && result.back() == L'\0')
                {
                    result.pop_back();
                }
                if (pattern.empty() || result.find(pattern) != std::wstring::npos)
                {
                    return result;
                }
            }
        }

        return {};
    }

    uint64_t ParseStateCounter(const std::wstring &state_text, const std::wstring &key)
    {
        const size_t begin = state_text.find(key);
        if (begin == std::wstring::npos)
        {
            return 0;
        }

        size_t cursor = begin + key.size();
        uint64_t value = 0;
        while (cursor < state_text.size() && state_text[cursor] >= L'0' && state_text[cursor] <= L'9')
        {
            value = value * 10u + static_cast<uint64_t>(state_text[cursor] - L'0');
            ++cursor;
        }
        return value;
    }
}
