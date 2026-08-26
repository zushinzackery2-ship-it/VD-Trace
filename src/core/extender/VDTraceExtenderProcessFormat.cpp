#include "pch.h"
#include "core/extender/VDTraceExtenderInternal.h"

namespace vdtrace::extender_detail
{
    namespace
    {
        bool IsInterestingSectionName(const wchar_t *name)
        {
            return name != nullptr
                && (_wcsicmp(name, L".data") == 0 || _wcsicmp(name, L".rdata") == 0);
        }
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
            const uint8_t value = bytes[index];
            text.push_back(value >= 0x20 && value <= 0x7e ? static_cast<char>(value) : '.');
        }
        return text;
    }

    std::string BuildExtendWriteSuffix(const extender::ExtendedMemoryAccess &access)
    {
        std::string target = access.operand_text;
        if (target.empty())
        {
            std::ostringstream address_out;
            address_out << "0x" << std::hex << access.memory_address;
            target = address_out.str();
        }

        std::ostringstream resolved_address;
        resolved_address << "0x" << std::hex << access.memory_address;
        std::ostringstream out;
        if (access.has_known_value && access.known_value_size != 0)
        {
            out << " --- [HeapPeek]->" << target
                << "=[" << resolved_address.str() << "]"
                << ".bytes=" << FormatBytes(access.known_value_bytes, access.known_value_size)
                << " ascii=\"" << FormatAscii(access.known_value_bytes, access.known_value_size) << "\""
                << " origin=extend.write";
            return out.str();
        }

        out << " --- [HeapPeekSkip]->" << target
            << "=[" << resolved_address.str() << "].reason=write-unknown"
            << " origin=extend.write";
        return out.str();
    }

    bool ResolveSectionRange(
        uintptr_t module_base,
        uintptr_t address,
        wchar_t *name,
        size_t name_capacity,
        uintptr_t &section_begin,
        uintptr_t &section_end)
    {
        section_begin = 0;
        section_end = 0;
        if (name == nullptr || name_capacity == 0 || module_base == 0 || address < module_base)
        {
            return false;
        }

        const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module_base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        {
            return false;
        }

        const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(module_base + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE)
        {
            return false;
        }

        const uintptr_t relative = address - module_base;
        const auto *section = IMAGE_FIRST_SECTION(nt);
        for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index)
        {
            const uintptr_t begin = section[index].VirtualAddress;
            const uintptr_t end = begin + std::max<DWORD>(section[index].Misc.VirtualSize, section[index].SizeOfRawData);
            if (relative < begin || relative >= end)
            {
                continue;
            }

            wchar_t buffer[16] = {};
            for (size_t cursor = 0; cursor < std::size(section[index].Name) && cursor + 1 < std::size(buffer); ++cursor)
            {
                const char value = static_cast<char>(section[index].Name[cursor]);
                if (value == '\0')
                {
                    break;
                }

                buffer[cursor] = static_cast<wchar_t>(value);
            }

            if (!IsInterestingSectionName(buffer))
            {
                return false;
            }

            wcsncpy_s(name, name_capacity, buffer, _TRUNCATE);
            section_begin = module_base + begin;
            section_end = module_base + end;
            return true;
        }

        return false;
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

    std::string BuildStaticWindowBlock(const StaticWindowCandidate &candidate)
    {
        std::vector<uint8_t> bytes(candidate.capture_size);
        if (!SafeReadMemoryBytes(candidate.capture_begin, bytes.data(), candidate.capture_size))
        {
            return {};
        }

        std::ostringstream out;
        out << "   [sample.static] rip=0x" << std::hex << candidate.instruction
            << " ea=0x" << candidate.address
            << std::dec;
        if (candidate.width != 0)
        {
            out << " width=" << static_cast<unsigned>(candidate.width);
        }
        if (candidate.from_address_only)
        {
            out << " source=lea";
        }
        out
            << " section=" << Narrow(std::wstring(candidate.section_name))
            << " window=0x" << std::hex << candidate.window_base
            << std::dec
            << " size=" << candidate.capture_size;
        if (!candidate.operand_text.empty())
        {
            out << " op=" << candidate.operand_text;
        }
        out << "\n";

        for (size_t offset = 0; offset < bytes.size(); offset += 16)
        {
            const size_t line_size = std::min<size_t>(16, bytes.size() - offset);
            out << "       0x" << std::hex << (candidate.capture_begin + offset)
                << "  " << FormatBytes(bytes.data() + offset, static_cast<uint8_t>(line_size))
                << " ascii=\"" << FormatAscii(bytes.data() + offset, static_cast<uint8_t>(line_size)) << "\"\n";
        }

        return out.str();
    }
}
