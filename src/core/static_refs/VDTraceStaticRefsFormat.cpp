#include "pch.h"
#include "core/static_refs/VDTraceStaticRefsInternal.h"

namespace vdtrace::static_refs_detail
{
    std::string Narrow(const std::wstring &text)
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

    std::string FormatResolvedAddressLabel(uintptr_t address)
    {
        AddressLabel label = {};
        if (!ResolveAddressLabel(address, label))
        {
            std::ostringstream out;
            out << "0x" << std::hex << address;
            return out.str();
        }

        std::ostringstream out;
        out << Narrow(label.module_name);
        if (!label.symbol_name.empty())
        {
            out << "!" << Narrow(label.symbol_name);
            if (label.symbol_offset != 0)
            {
                out << "+0x" << std::hex << label.symbol_offset;
            }
        }
        else
        {
            out << "+0x" << std::hex << label.relative;
        }
        return out.str();
    }

    bool TryGuessObjectWithVftable(const uint8_t *bytes, uint8_t size, std::string &detail)
    {
        detail.clear();
        if (bytes == nullptr || size < sizeof(uintptr_t))
        {
            return false;
        }

        uintptr_t vftable = 0;
        std::memcpy(&vftable, bytes, sizeof(vftable));
        if (!LooksLikePointerValue(vftable))
        {
            return false;
        }

        uintptr_t first_entry = 0;
        if (!SafeReadMemoryBytes(vftable, &first_entry, sizeof(first_entry)) || !IsExecutableAddress(first_entry))
        {
            return false;
        }

        detail = "vftable=" + FormatResolvedAddressLabel(vftable) + " first=" + FormatResolvedAddressLabel(first_entry);
        return true;
    }

    bool TryGuessPointerArray(const uint8_t *bytes, uint8_t size, std::string &detail)
    {
        detail.clear();
        if (bytes == nullptr || size < sizeof(uintptr_t) * 2)
        {
            return false;
        }

        uintptr_t first = 0;
        uintptr_t second = 0;
        std::memcpy(&first, bytes, sizeof(first));
        std::memcpy(&second, bytes + sizeof(second), sizeof(second));
        if (!LooksLikePointerValue(first) || !LooksLikePointerValue(second))
        {
            return false;
        }

        MEMORY_BASIC_INFORMATION first_info = {};
        MEMORY_BASIC_INFORMATION second_info = {};
        if (!IsReadableAddress(first, first_info) || !IsReadableAddress(second, second_info))
        {
            return false;
        }

        detail = "first=" + FormatResolvedAddressLabel(first) + " second=" + FormatResolvedAddressLabel(second);
        return true;
    }

    std::string FormatBytes(const uint8_t *bytes, uint8_t size)
    {
        std::ostringstream out;
        for (uint8_t index = 0; index < size; ++index)
        {
            if (index != 0)
            {
                out << ' ';
            }
            out << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(bytes[index]);
        }
        return out.str();
    }

    std::string FormatAscii(const uint8_t *bytes, uint8_t size)
    {
        std::string text;
        text.reserve(size);
        for (uint8_t index = 0; index < size; ++index)
        {
            text.push_back(IsPrintableAscii(bytes[index]) ? static_cast<char>(bytes[index]) : '.');
        }
        return text;
    }
}
