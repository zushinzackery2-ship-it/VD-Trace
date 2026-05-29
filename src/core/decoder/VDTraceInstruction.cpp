#include "pch.h"
#include "core/runtime/VDTraceInternal.h"
#include "third_party/zydis/Zydis.h"

namespace vdtrace
{
    namespace
    {
        ZydisDecoder &GetDecoder()
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

        bool SafeReadInstructionBytes(uintptr_t instruction, uint8_t *buffer, size_t capacity, uint8_t &size)
        {
            size = 0;
            if (buffer == nullptr || capacity == 0)
            {
                return false;
            }

            if (!SafeReadMemoryBytes(instruction, buffer, capacity))
            {
                return false;
            }

            size = static_cast<uint8_t>(capacity);
            return true;
        }

        uintptr_t ComputeRelativeTarget(uintptr_t instruction, uint8_t instruction_size, int32_t displacement)
        {
            return instruction + instruction_size + displacement;
        }

        EventKind ClassifyMnemonic(const ZydisDecodedInstruction &instruction)
        {
            switch (instruction.mnemonic)
            {
            case ZYDIS_MNEMONIC_CALL:
                return EventKind::Call;

            case ZYDIS_MNEMONIC_JMP:
            case ZYDIS_MNEMONIC_JMPABS:
                return EventKind::Jump;

            case ZYDIS_MNEMONIC_RET:
            case ZYDIS_MNEMONIC_IRET:
            case ZYDIS_MNEMONIC_IRETD:
            case ZYDIS_MNEMONIC_IRETQ:
                return EventKind::Return;

            case ZYDIS_MNEMONIC_SYSCALL:
                return EventKind::Syscall;

            case ZYDIS_MNEMONIC_INT:
            case ZYDIS_MNEMONIC_INT1:
            case ZYDIS_MNEMONIC_INT3:
            case ZYDIS_MNEMONIC_INTO:
                return EventKind::Interrupt;

            default:
                break;
            }

            if (instruction.meta.branch_type != ZYDIS_BRANCH_TYPE_NONE)
            {
                return EventKind::ConditionalJump;
            }

            return EventKind::Other;
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
                if (operand.type == ZYDIS_OPERAND_TYPE_IMMEDIATE
                    && (operand.imm.is_relative || operand.imm.is_address))
                {
                    ZyanU64 absolute_address = 0;
                    if (ZYAN_SUCCESS(ZydisCalcAbsoluteAddress(&instruction, &operand, runtime_address, &absolute_address)))
                    {
                        target = static_cast<uintptr_t>(absolute_address);
                        return true;
                    }

                    if (operand.imm.is_relative)
                    {
                        target = ComputeRelativeTarget(
                            runtime_address,
                            instruction.length,
                            static_cast<int32_t>(operand.imm.value.s));
                        return true;
                    }

                    target = static_cast<uintptr_t>(operand.imm.value.u);
                    return true;
                }
            }

            return false;
        }
    }

    bool SafeReadMemoryBytes(uintptr_t address, void *buffer, size_t size)
    {
        if (buffer == nullptr || size == 0)
        {
            return false;
        }

        __try
        {
            const auto *source = reinterpret_cast<const uint8_t *>(address);
            auto *destination = static_cast<uint8_t *>(buffer);
            std::memcpy(destination, source, size);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    InstructionDecodeResult DecodeInstruction(uintptr_t instruction)
    {
        InstructionDecodeResult result = {};
        uint8_t size = 0;
        if (!SafeReadInstructionBytes(instruction, result.bytes, sizeof(result.bytes), size))
        {
            return result;
        }

        ZydisDecodedInstruction decoded = {};
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
        if (!ZYAN_SUCCESS(ZydisDecoderDecodeFull(
                &GetDecoder(),
                result.bytes,
                size,
                &decoded,
                operands)))
        {
            return result;
        }

        result.size = decoded.length;
        result.kind = ClassifyMnemonic(decoded);
        uintptr_t target = 0;
        if (TryResolveDirectTarget(instruction, decoded, operands, target))
        {
            result.has_target = true;
            result.target = target;
        }
        return result;
    }
}
