#include "pch.h"
#include "core/static_refs/VDTraceStaticRefsInternal.h"

namespace vdtrace::static_refs_detail
{
    ZydisDecoder &GetStaticRefDecoder()
    {
        static ZydisDecoder decoder = {};
        static bool initialized = false;
        if (!initialized)
        {
            ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);
            initialized = true;
        }
        return decoder;
    }

    bool IsInterestingSectionName(const wchar_t *name)
    {
        if (name == nullptr)
        {
            return false;
        }

        return _wcsicmp(name, L".data") == 0
            || _wcsicmp(name, L".rdata") == 0;
    }

    bool ResolveSectionName(uintptr_t module_base, uintptr_t address, wchar_t *name, size_t capacity)
    {
        if (name == nullptr || capacity == 0 || module_base == 0 || address < module_base)
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

        const auto *section = IMAGE_FIRST_SECTION(nt);
        const uintptr_t relative = address - module_base;
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

            wcsncpy_s(name, capacity, buffer, _TRUNCATE);
            return true;
        }

        return false;
    }

    bool IsPrintableAscii(uint8_t value)
    {
        return value >= 0x20 && value <= 0x7e;
    }

    bool LooksLikeString(const uint8_t *bytes, uint8_t size)
    {
        if (bytes == nullptr || size < 8)
        {
            return false;
        }

        size_t printable = 0;
        bool saw_zero = false;
        for (uint8_t index = 0; index < size; ++index)
        {
            if (bytes[index] == 0)
            {
                saw_zero = true;
                break;
            }

            if (!IsPrintableAscii(bytes[index]))
            {
                return false;
            }
            ++printable;
        }

        return printable >= 6 && saw_zero;
    }

    bool IsExecutableAddress(uintptr_t address)
    {
        MEMORY_BASIC_INFORMATION information = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &information, sizeof(information)) == 0)
        {
            return false;
        }

        const DWORD protection = information.Protect & 0xff;
        return protection == PAGE_EXECUTE
            || protection == PAGE_EXECUTE_READ
            || protection == PAGE_EXECUTE_READWRITE
            || protection == PAGE_EXECUTE_WRITECOPY;
    }

    bool IsReadableAddress(uintptr_t address, MEMORY_BASIC_INFORMATION &information)
    {
        information = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &information, sizeof(information)) == 0)
        {
            return false;
        }

        if (information.State != MEM_COMMIT
            || (information.Protect & PAGE_GUARD) != 0
            || (information.Protect & PAGE_NOACCESS) != 0)
        {
            return false;
        }

        return true;
    }

    bool LooksLikePointerValue(uintptr_t value)
    {
        return value >= 0x10000u && value != std::numeric_limits<uintptr_t>::max();
    }

    bool LooksLikeCodePointerTable(const uint8_t *bytes, uint8_t size)
    {
        if (bytes == nullptr || size < sizeof(uintptr_t))
        {
            return false;
        }

        uintptr_t first = 0;
        std::memcpy(&first, bytes, sizeof(first));
        if (!IsExecutableAddress(first))
        {
            return false;
        }

        if (size >= sizeof(uintptr_t) * 2)
        {
            uintptr_t second = 0;
            std::memcpy(&second, bytes + sizeof(uintptr_t), sizeof(second));
            return IsExecutableAddress(second);
        }

        return true;
    }

    std::string EscapePreviewText(const std::string &text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (char value : text)
        {
            if (value == '\\' || value == '"')
            {
                escaped.push_back('\\');
            }
            if (value == '\r' || value == '\n' || value == '\t')
            {
                escaped.push_back(' ');
            }
            else
            {
                escaped.push_back(value);
            }
        }
        return escaped;
    }

    bool TryDecodeAsciiString(const uint8_t *bytes, uint8_t size, std::string &text)
    {
        text.clear();
        if (bytes == nullptr || size < 4)
        {
            return false;
        }

        bool saw_terminator = false;
        for (uint8_t index = 0; index < size; ++index)
        {
            const uint8_t value = bytes[index];
            if (value == 0)
            {
                saw_terminator = true;
                break;
            }
            if (!IsPrintableAscii(value))
            {
                return false;
            }
            text.push_back(static_cast<char>(value));
        }

        return saw_terminator && text.size() >= 4;
    }

    bool TryDecodeUtf16String(const uint8_t *bytes, uint8_t size, std::string &text)
    {
        text.clear();
        if (bytes == nullptr || size < 8 || (size % 2) != 0)
        {
            return false;
        }

        bool saw_terminator = false;
        for (uint8_t index = 0; index + 1 < size; index += 2)
        {
            const uint8_t low = bytes[index];
            const uint8_t high = bytes[index + 1];
            if (low == 0 && high == 0)
            {
                saw_terminator = true;
                break;
            }
            if (high != 0 || !IsPrintableAscii(low))
            {
                return false;
            }
            text.push_back(static_cast<char>(low));
        }

        return saw_terminator && text.size() >= 4;
    }

}
