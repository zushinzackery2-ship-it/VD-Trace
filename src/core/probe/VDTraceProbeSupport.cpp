#include "pch.h"
#include "core/probe/VDTraceProbeInternal.h"

namespace vdtrace::probe_detail
{
    std::wstring TrimText(const std::wstring &text)
    {
        const size_t begin = text.find_first_not_of(L" \t\r\n");
        if (begin == std::wstring::npos)
        {
            return {};
        }

        const size_t end = text.find_last_not_of(L" \t\r\n");
        return text.substr(begin, end - begin + 1);
    }

    std::wstring ToLowerCopy(const std::wstring &text)
    {
        std::wstring lowered = text;
        std::transform(
            lowered.begin(),
            lowered.end(),
            lowered.begin(),
            [](wchar_t value)
            {
                return static_cast<wchar_t>(towlower(value));
            });
        return lowered;
    }

    std::vector<std::wstring> SplitText(const std::wstring &text, wchar_t separator)
    {
        std::vector<std::wstring> parts;
        size_t begin = 0;
        while (begin <= text.size())
        {
            const size_t end = text.find(separator, begin);
            const std::wstring part = TrimText(text.substr(begin, end == std::wstring::npos ? std::wstring::npos : end - begin));
            if (!part.empty())
            {
                parts.push_back(part);
            }

            if (end == std::wstring::npos)
            {
                break;
            }
            begin = end + 1;
        }
        return parts;
    }

    bool ParseUnsignedText(const std::wstring &text, uintptr_t &value)
    {
        const std::wstring trimmed = TrimText(text);
        if (trimmed.empty())
        {
            value = 0;
            return false;
        }

        wchar_t *end = nullptr;
        const unsigned long long parsed = wcstoull(trimmed.c_str(), &end, 0);
        if (end == trimmed.c_str() || (end != nullptr && *end != L'\0'))
        {
            value = 0;
            return false;
        }

        value = static_cast<uintptr_t>(parsed);
        return true;
    }

    bool ParseSignedText(const std::wstring &text, intptr_t &value)
    {
        const std::wstring trimmed = TrimText(text);
        if (trimmed.empty())
        {
            value = 0;
            return false;
        }

        wchar_t *end = nullptr;
        const long long parsed = _wcstoi64(trimmed.c_str(), &end, 0);
        if (end == trimmed.c_str() || (end != nullptr && *end != L'\0'))
        {
            value = 0;
            return false;
        }

        value = static_cast<intptr_t>(parsed);
        return true;
    }

    bool ResolveConfiguredAddressText(const std::wstring &text, uintptr_t &address, std::wstring &error)
    {
        error.clear();
        address = 0;

        const std::wstring raw = TrimText(text);
        if (raw.empty())
        {
            error = L"地址文本为空。";
            return false;
        }

        const size_t split = raw.find_first_of(L"!+");
        if (split == std::wstring::npos)
        {
            if (!ParseUnsignedText(raw, address))
            {
                error = L"地址格式无效。使用 0xADDRESS 或 module!0xRVA。";
                return false;
            }
            return true;
        }

        const std::wstring module_name = TrimText(raw.substr(0, split));
        uintptr_t relative = 0;
        if (module_name.empty() || !ParseUnsignedText(raw.substr(split + 1), relative))
        {
            error = L"地址格式无效。使用 module!0xRVA 或 module+0xRVA。";
            return false;
        }

        ModuleRange range = {};
        if (!ResolveModuleRange(module_name, range, error))
        {
            return false;
        }

        address = range.base + relative;
        return true;
    }

    bool ParseProbeRegister(const std::wstring &text, ProbeRegister &reg)
    {
        const std::wstring lowered = ToLowerCopy(TrimText(text));
        if (lowered == L"rax") reg = ProbeRegister::Rax;
        else if (lowered == L"rbx") reg = ProbeRegister::Rbx;
        else if (lowered == L"rcx") reg = ProbeRegister::Rcx;
        else if (lowered == L"rdx") reg = ProbeRegister::Rdx;
        else if (lowered == L"rsi") reg = ProbeRegister::Rsi;
        else if (lowered == L"rdi") reg = ProbeRegister::Rdi;
        else if (lowered == L"rbp") reg = ProbeRegister::Rbp;
        else if (lowered == L"rsp") reg = ProbeRegister::Rsp;
        else if (lowered == L"rip") reg = ProbeRegister::Rip;
        else if (lowered == L"r8") reg = ProbeRegister::R8;
        else if (lowered == L"r9") reg = ProbeRegister::R9;
        else if (lowered == L"r10") reg = ProbeRegister::R10;
        else if (lowered == L"r11") reg = ProbeRegister::R11;
        else if (lowered == L"r12") reg = ProbeRegister::R12;
        else if (lowered == L"r13") reg = ProbeRegister::R13;
        else if (lowered == L"r14") reg = ProbeRegister::R14;
        else if (lowered == L"r15") reg = ProbeRegister::R15;
        else
        {
            reg = ProbeRegister::None;
            return false;
        }

        return true;
    }

    bool ParseProbeRegisterExpression(const std::wstring &text, ProbeRegister &reg, intptr_t &offset)
    {
        const std::wstring trimmed = TrimText(text);
        const size_t split = trimmed.find_first_of(L"+-", 1);
        if (split == std::wstring::npos)
        {
            offset = 0;
            return ParseProbeRegister(trimmed, reg);
        }

        if (!ParseProbeRegister(trimmed.substr(0, split), reg))
        {
            return false;
        }

        return ParseSignedText(trimmed.substr(split), offset);
    }

    bool CopyProbeLabel(const std::wstring &text, wchar_t *buffer, size_t capacity)
    {
        if (buffer == nullptr || capacity == 0)
        {
            return false;
        }

        wmemset(buffer, 0, capacity);
        if (text.empty())
        {
            return true;
        }

        return wcsncpy_s(buffer, capacity, text.c_str(), _TRUNCATE) == 0;
    }

    bool ParseProbeSize(const std::wstring &text, uint8_t &size)
    {
        uintptr_t parsed = 0;
        if (!ParseUnsignedText(text, parsed) || parsed == 0 || parsed > kEnhancedSampleMaxBytes)
        {
            size = 0;
            return false;
        }

        size = static_cast<uint8_t>(parsed);
        return true;
    }

    bool ParseProbeCaptureToken(const std::wstring &text, ResolvedProbeCapture &capture, std::wstring &error)
    {
        capture = {};
        const std::vector<std::wstring> parts = SplitText(text, L':');
        if (parts.size() < 2)
        {
            error = L"capture 语法无效。使用 reg:rcx[:label] / mem:addr:size[:label] / ptr:rcx+0x10:size[:label]";
            return false;
        }

        const std::wstring kind = ToLowerCopy(parts[0]);
        if (kind == L"reg")
        {
            if (parts.size() > 3 || !ParseProbeRegister(parts[1], capture.reg))
            {
                error = L"reg capture 无效。";
                return false;
            }

            capture.kind = ProbeSourceKind::RegisterValue;
            const std::wstring label = parts.size() >= 3 ? parts[2] : parts[1];
            CopyProbeLabel(label, capture.label, std::size(capture.label));
            return true;
        }

        if (kind == L"mem")
        {
            if (parts.size() < 3 || parts.size() > 4)
            {
                error = L"mem capture 无效。";
                return false;
            }

            if (!ResolveConfiguredAddressText(parts[1], capture.absolute_address, error)
                || !ParseProbeSize(parts[2], capture.size))
            {
                if (error.empty())
                {
                    error = L"mem capture 无效。";
                }
                return false;
            }

            capture.kind = ProbeSourceKind::AbsoluteMemory;
            const std::wstring label = parts.size() >= 4 ? parts[3] : parts[1];
            CopyProbeLabel(label, capture.label, std::size(capture.label));
            return true;
        }

        if (kind == L"ptr")
        {
            if (parts.size() < 3 || parts.size() > 4)
            {
                error = L"ptr capture 无效。";
                return false;
            }

            if (!ParseProbeRegisterExpression(parts[1], capture.reg, capture.offset)
                || !ParseProbeSize(parts[2], capture.size))
            {
                error = L"ptr capture 无效。";
                return false;
            }

            capture.kind = ProbeSourceKind::RegisterMemory;
            const std::wstring label = parts.size() >= 4 ? parts[3] : parts[1];
            CopyProbeLabel(label, capture.label, std::size(capture.label));
            return true;
        }

        error = L"未知 capture 类型。";
        return false;
    }
}
