#include "pch.h"
#include "core/runtime/VDTraceInternal.h"

namespace vdtrace
{
    namespace
    {
        constexpr size_t kMaxPreviewBytes = 16;
        constexpr uintptr_t kMinPointerPreviewAddress = 0x10000;

        bool IsExecutableProtection(DWORD protection)
        {
            const DWORD basic = protection & 0xff;
            return basic == PAGE_EXECUTE
                || basic == PAGE_EXECUTE_READ
                || basic == PAGE_EXECUTE_READWRITE
                || basic == PAGE_EXECUTE_WRITECOPY;
        }

        bool IsReadableProtection(DWORD protection)
        {
            if ((protection & PAGE_GUARD) != 0 || (protection & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            const DWORD basic = protection & 0xff;
            return basic == PAGE_READONLY
                || basic == PAGE_READWRITE
                || basic == PAGE_WRITECOPY
                || basic == PAGE_EXECUTE_READ
                || basic == PAGE_EXECUTE_READWRITE
                || basic == PAGE_EXECUTE_WRITECOPY;
        }

        bool TryReadPreviewBytes(uintptr_t address, uint8_t *buffer, size_t &size)
        {
            size = 0;
            if (address < kMinPointerPreviewAddress)
            {
                return false;
            }

            MEMORY_BASIC_INFORMATION information = {};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &information, sizeof(information)) == 0)
            {
                return false;
            }

            if (information.State != MEM_COMMIT
                || !IsReadableProtection(information.Protect)
                || IsExecutableProtection(information.Protect))
            {
                return false;
            }

            const uintptr_t region_base = reinterpret_cast<uintptr_t>(information.BaseAddress);
            const size_t region_offset = address >= region_base ? static_cast<size_t>(address - region_base) : 0;
            const size_t available = information.RegionSize > region_offset ? information.RegionSize - region_offset : 0;
            if (available == 0)
            {
                return false;
            }

            size = std::min(kMaxPreviewBytes, available);
            return SafeReadMemoryBytes(address, buffer, size);
        }

        std::string FormatHexValueLocal(uintptr_t value)
        {
            std::ostringstream out;
            out << "0x" << std::hex << value;
            return out.str();
        }

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

        bool TryFormatAnonymousExecLabel(uintptr_t address, std::string &text)
        {
            text.clear();
            if (address == 0)
            {
                return false;
            }

            MEMORY_BASIC_INFORMATION mbi = {};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0)
            {
                return false;
            }

            if (mbi.State != MEM_COMMIT || mbi.Type == MEM_IMAGE || !IsExecutableProtection(mbi.Protect))
            {
                return false;
            }

            const uintptr_t identity = reinterpret_cast<uintptr_t>(mbi.AllocationBase != nullptr ? mbi.AllocationBase : mbi.BaseAddress);
            std::ostringstream out;
            out << "anon-exec@0x" << std::hex << identity;
            if (address > identity)
            {
                out << "+0x" << (address - identity);
            }
            text = out.str();
            return true;
        }

        std::string ResolveAddressLabelTextLocal(
            uintptr_t address,
            std::unordered_map<uintptr_t, std::string> &address_label_cache,
            const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map)
        {
            if (address == 0)
            {
                return "0x0";
            }

            const auto async_it = async_probe_map.find(address);
            if (async_it != async_probe_map.end())
            {
                return Narrow(async_it->second.module_name) + "!" + Narrow(async_it->second.symbol_name);
            }

            const auto cached = address_label_cache.find(address);
            if (cached != address_label_cache.end())
            {
                return cached->second;
            }

            AddressLabel label = {};
            if (!ResolveAddressLabel(address, label))
            {
                std::string fallback;
                if (!TryFormatAnonymousExecLabel(address, fallback))
                {
                    fallback = FormatHexValueLocal(address);
                }
                address_label_cache.emplace(address, fallback);
                return fallback;
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
            const std::string result = out.str();
            address_label_cache.emplace(address, result);
            return result;
        }

        std::string FormatPreviewBytes(const uint8_t *bytes, size_t size)
        {
            std::ostringstream out;
            for (size_t index = 0; index < size; ++index)
            {
                if (index != 0)
                {
                    out << ' ';
                }
                out << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(bytes[index]);
            }
            return out.str();
        }

        std::string FormatPreviewAscii(const uint8_t *bytes, size_t size)
        {
            std::string text;
            text.reserve(size);
            for (size_t index = 0; index < size; ++index)
            {
                const uint8_t value = bytes[index];
                text.push_back(value >= 0x20 && value <= 0x7e ? static_cast<char>(value) : '.');
            }
            return text;
        }
    }

    std::string FormatValueWithPreview(
        uintptr_t value,
        bool prefer_symbol,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map)
    {
        std::string formatted = prefer_symbol
            ? ResolveAddressLabelTextLocal(value, address_label_cache, async_probe_map)
            : FormatHexValueLocal(value);
        if (!prefer_symbol && value >= kMinPointerPreviewAddress)
        {
            AddressLabel label = {};
            if (ResolveAddressLabel(value, label))
            {
                formatted = ResolveAddressLabelTextLocal(value, address_label_cache, async_probe_map);
            }
        }

        uint8_t preview[kMaxPreviewBytes] = {};
        size_t preview_size = 0;
        if (!TryReadPreviewBytes(value, preview, preview_size) || preview_size == 0)
        {
            return formatted;
        }

        std::ostringstream out;
        out << formatted
            << " {mem=" << FormatPreviewBytes(preview, preview_size)
            << " ascii=\"" << FormatPreviewAscii(preview, preview_size) << "\"}";
        return out.str();
    }
}
