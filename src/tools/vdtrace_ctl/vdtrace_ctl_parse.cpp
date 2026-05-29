#include "vdtrace_ctl_internal.h"

#include <cstring>

namespace vdtrace::tools::cli
{
    bool ParseDword(const wchar_t *text, DWORD &value)
    {
        if (text == nullptr)
        {
            return false;
        }

        wchar_t *end = nullptr;
        const unsigned long parsed = wcstoul(text, &end, 10);
        if (end == text || (end != nullptr && *end != L'\0'))
        {
            return false;
        }

        value = static_cast<DWORD>(parsed);
        return true;
    }

    bool ParseUint64(const wchar_t *text, uint64_t &value)
    {
        if (text == nullptr)
        {
            return false;
        }

        wchar_t *end = nullptr;
        value = wcstoull(text, &end, 10);
        return !(end == text || (end != nullptr && *end != L'\0'));
    }

    bool ParseCallDepthText(const std::wstring &text, uint32_t &value)
    {
        if (_wcsicmp(text.c_str(), L"all") == 0)
        {
            value = vdtrace::kUnlimitedCallDepth;
            return true;
        }

        if (_wcsicmp(text.c_str(), L"same") == 0 || _wcsicmp(text.c_str(), L"single") == 0)
        {
            value = 0;
            return true;
        }

        uint64_t parsed = 0;
        if (!ParseUint64(text.c_str(), parsed) || parsed > vdtrace::kUnlimitedCallDepth)
        {
            return false;
        }

        value = static_cast<uint32_t>(parsed);
        return true;
    }

    bool ParseHexBytes(const std::wstring &text, std::vector<uint8_t> &bytes)
    {
        bytes.clear();
        std::wstring compact;
        compact.reserve(text.size());
        for (wchar_t value : text)
        {
            if (value == L' ' || value == L'\t' || value == L'\r' || value == L'\n' || value == L',' || value == L'-')
            {
                continue;
            }
            compact.push_back(value);
        }

        if (compact.empty() || (compact.size() % 2) != 0)
        {
            return false;
        }

        for (size_t index = 0; index < compact.size(); index += 2)
        {
            wchar_t pair[3] = {compact[index], compact[index + 1], L'\0'};
            wchar_t *end = nullptr;
            const unsigned long parsed = wcstoul(pair, &end, 16);
            if (end == pair || (end != nullptr && *end != L'\0') || parsed > 0xFFul)
            {
                bytes.clear();
                return false;
            }
            bytes.push_back(static_cast<uint8_t>(parsed));
        }

        return !bytes.empty();
    }

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
}
