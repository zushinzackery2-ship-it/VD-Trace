#include "pch.h"
#include "core/heap_peek/VDTraceHeapPeekSupportInternal.h"

namespace vdtrace::heap_peek::detail
{
    ZydisDecoder &GetHeapPeekDecoder()
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

    ZydisFormatter &GetHeapPeekFormatter()
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

    uint8_t ClampPeekSizeBits(uint32_t bits)
    {
        if (bits == 0)
        {
            return 0;
        }
        return static_cast<uint8_t>(std::min<uint32_t>((bits + 7u) / 8u, kEnhancedSampleMaxBytes));
    }

    uint8_t RegisterPeekSizeBytes(ZydisRegister reg)
    {
        uint32_t width = ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, reg);
        if (width == 0)
        {
            const ZydisRegister enclosed = ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64, reg);
            width = ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, enclosed);
        }
        return ClampPeekSizeBits(width);
    }

    bool TryReadPeekBytes(const HeapPeekRequest &request, uint8_t *bytes, uint8_t &size)
    {
        if (request.memory_address == 0 || !IsHeapLikeAddress(request.memory_address, request.stack_pointer))
        {
            return false;
        }

        size = std::min<uint8_t>(request.peek_size, kEnhancedSampleMaxBytes);
        if (size == 0)
        {
            size = 16;
        }

        return SafeReadMemoryBytes(request.memory_address, bytes, size);
    }

    void FillRegisterContext(const ThreadContextSnapshot &snapshot, ZydisRegisterContext &context)
    {
        std::memset(&context, 0, sizeof(context));
        context.values[ZYDIS_REGISTER_RAX] = snapshot.rax;
        context.values[ZYDIS_REGISTER_RBX] = snapshot.rbx;
        context.values[ZYDIS_REGISTER_RCX] = snapshot.rcx;
        context.values[ZYDIS_REGISTER_RDX] = snapshot.rdx;
        context.values[ZYDIS_REGISTER_RSP] = snapshot.rsp;
        context.values[ZYDIS_REGISTER_RBP] = snapshot.rbp;
        context.values[ZYDIS_REGISTER_RSI] = snapshot.rsi;
        context.values[ZYDIS_REGISTER_RDI] = snapshot.rdi;
        context.values[ZYDIS_REGISTER_R8] = snapshot.r8;
        context.values[ZYDIS_REGISTER_R9] = snapshot.r9;
        context.values[ZYDIS_REGISTER_R10] = snapshot.r10;
        context.values[ZYDIS_REGISTER_R11] = snapshot.r11;
        context.values[ZYDIS_REGISTER_R12] = snapshot.r12;
        context.values[ZYDIS_REGISTER_R13] = snapshot.r13;
        context.values[ZYDIS_REGISTER_R14] = snapshot.r14;
        context.values[ZYDIS_REGISTER_R15] = snapshot.r15;
    }

    const char *OriginText(HeapPeekOrigin origin)
    {
        switch (origin)
        {
        case HeapPeekOrigin::BlockExtendRead:
            return "extend.read";
        case HeapPeekOrigin::BlockExtendWrite:
            return "extend.write";
        case HeapPeekOrigin::BlockExtendReadWrite:
            return "extend.rw";
        case HeapPeekOrigin::DirectEvent:
        default:
            return nullptr;
        }
    }

    bool TryBuildHeapPeekRequestCore(
        const RecorderQueuedEvent &event,
        HeapPeekRequest &request,
        std::string *operand_text)
    {
        request = {};
        if (!event.thread_context.valid
            || event.instruction_size == 0
            || event.kind == EventKind::Probe
            || event.kind == EventKind::Other
            || event.kind == EventKind::Unknown)
        {
            return false;
        }

        ZydisDecodedInstruction instruction = {};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                &GetHeapPeekDecoder(),
                event.instruction_bytes,
                sizeof(event.instruction_bytes),
                &instruction,
                operands)))
        {
            return false;
        }

        const bool address_only =
            instruction.mnemonic == ZYDIS_MNEMONIC_LEA
            || instruction.mnemonic == ZYDIS_MNEMONIC_NOP;

        ZydisRegisterContext register_context = {};
        FillRegisterContext(event.thread_context, register_context);
        for (uint8_t index = 0; index < instruction.operand_count_visible; ++index)
        {
            const ZydisDecodedOperand &operand = operands[index];
            if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY || operand.mem.type != ZYDIS_MEMOP_TYPE_MEM)
            {
                continue;
            }

            const ZyanU8 reads = operand.actions & ZYDIS_OPERAND_ACTION_MASK_READ;
            const ZyanU8 writes = operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE;
            if (!address_only && reads == 0 && writes == 0)
            {
                continue;
            }

            ZyanU64 absolute = 0;
            if (!ZYAN_SUCCESS(ZydisCalcAbsoluteAddressEx(
                    &instruction,
                    &operand,
                    event.instruction,
                    &register_context,
                    &absolute)))
            {
                continue;
            }

            request.sequence = event.sequence;
            request.thread_id = event.thread_id;
            request.kind = event.kind;
            request.instruction = event.instruction;
            request.memory_address = static_cast<uintptr_t>(absolute);
            request.stack_pointer = event.thread_context.rsp;
            request.peek_size = ResolveHeapPeekSize(instruction, operands, index, address_only);
            request.origin = HeapPeekOrigin::DirectEvent;

            if (operand_text != nullptr)
            {
                char text[128] = {};
                if (ZYAN_SUCCESS(ZydisFormatterFormatOperand(
                        &GetHeapPeekFormatter(),
                        &instruction,
                        &operand,
                        text,
                        sizeof(text),
                        event.instruction,
                        nullptr)))
                {
                    *operand_text = text;
                }
                else
                {
                    operand_text->clear();
                }
            }
            return true;
        }

        return false;
    }
}
