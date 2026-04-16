#include "pch.h"
#include "VDTraceFunctionPreviewInternal.h"

namespace vdtrace::function_preview_detail
{
    ZydisDecoder &GetPreviewDecoder()
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

    ZydisFormatter &GetPreviewFormatter()
    {
        static ZydisFormatter formatter = {};
        static bool initialized = false;
        if (!initialized)
        {
            ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL);
            initialized = true;
        }
        return formatter;
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

    bool IsExecutableProtection(DWORD protection)
    {
        const DWORD basic = protection & 0xff;
        return basic == PAGE_EXECUTE
            || basic == PAGE_EXECUTE_READ
            || basic == PAGE_EXECUTE_READWRITE
            || basic == PAGE_EXECUTE_WRITECOPY;
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

    std::string ResolveAddressLabelText(
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
                std::ostringstream out;
                out << "0x" << std::hex << address;
                fallback = out.str();
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

    bool IsPreviewTerminator(const ZydisDecodedInstruction &instruction)
    {
        switch (instruction.mnemonic)
        {
        case ZYDIS_MNEMONIC_RET:
        case ZYDIS_MNEMONIC_IRET:
        case ZYDIS_MNEMONIC_IRETD:
        case ZYDIS_MNEMONIC_IRETQ:
        case ZYDIS_MNEMONIC_JMP:
        case ZYDIS_MNEMONIC_JMPABS:
        case ZYDIS_MNEMONIC_INT:
        case ZYDIS_MNEMONIC_INT1:
        case ZYDIS_MNEMONIC_INT3:
            return true;
        default:
            return false;
        }
    }

    std::string FormatInstructionBytes(const uint8_t *bytes, uint8_t size)
    {
        std::ostringstream out;
        for (uint8_t i = 0; i < size; i++)
        {
            if (i != 0)
            {
                out << ' ';
            }
            out << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(bytes[i]);
        }
        return out.str();
    }

    bool TryResolveDirectTarget(
        uintptr_t runtime_address,
        const ZydisDecodedInstruction &instruction,
        const ZydisDecodedOperand *operands,
        uintptr_t &target)
    {
        target = 0;
        for (uint8_t index = 0; index < instruction.operand_count_visible; index++)
        {
            const ZydisDecodedOperand &operand = operands[index];
            if (operand.type != ZYDIS_OPERAND_TYPE_IMMEDIATE
                || (!operand.imm.is_relative && !operand.imm.is_address))
            {
                continue;
            }

            ZyanU64 absolute_address = 0;
            if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction, &operand, runtime_address, &absolute_address)))
            {
                target = static_cast<uintptr_t>(absolute_address);
                return true;
            }

            if (operand.imm.is_relative)
            {
                target = runtime_address + instruction.length + static_cast<int32_t>(operand.imm.value.s);
                return true;
            }

            target = static_cast<uintptr_t>(operand.imm.value.u);
            return true;
        }

        return false;
    }

    bool IsAddressInsidePreviewRange(const PreviewRange &range, uintptr_t address)
    {
        return range.valid && range.size != 0 && address >= range.base && address < (range.base + range.size);
    }

    bool IsInsidePreviewWindow(uintptr_t entry, uintptr_t address)
    {
        return address >= entry
            ? (address - entry) <= kMaxPreviewAddressDistance
            : (entry - address) <= kMaxPreviewAddressDistance;
    }

    PreviewRange ResolvePreviewRange(uintptr_t entry)
    {
        PreviewRange range = {};

        AddressLabel module = {};
        if (ResolveAddressLabel(entry, module) && module.valid && module.module_size != 0)
        {
            range.valid = true;
            range.base = module.module_base;
            range.size = module.module_size;
            return range;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(entry), &mbi, sizeof(mbi)) == 0)
        {
            return range;
        }

        if (mbi.State != MEM_COMMIT || mbi.Type == MEM_IMAGE || !IsExecutableProtection(mbi.Protect))
        {
            return range;
        }

        range.valid = true;
        range.base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        range.size = mbi.RegionSize;
        return range;
    }

    bool TryBuildPreviewEntryContext(const RecorderQueuedEvent &event, ThreadContextSnapshot &entry_context)
    {
        if (!event.thread_context.valid || !event.has_target || event.target == 0)
        {
            return false;
        }

        entry_context = event.thread_context;
        entry_context.valid = true;
        entry_context.rip = event.target;
        if (event.kind == EventKind::Call && entry_context.rsp >= sizeof(uintptr_t))
        {
            entry_context.rsp -= sizeof(uintptr_t);
        }
        return true;
    }

    uintptr_t FindPreviewEntryBlockEnd(uintptr_t entry, const std::vector<PreviewInstruction> &instructions)
    {
        if (instructions.empty())
        {
            return entry;
        }

        uintptr_t cursor = entry;
        for (const PreviewInstruction &instruction : instructions)
        {
            if (instruction.address != cursor)
            {
                break;
            }
            cursor += instruction.size;
        }
        return cursor;
    }

}
