#include "pch.h"
#include "core/extender/VDTraceExtenderAnalyzeInternal.h"

namespace vdtrace
{
    namespace extender
    {
        namespace detail
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

            ZydisFormatter &GetFormatter()
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

            bool TryComputeOperandAddress(
                const SimContext &context,
                const ZydisDecodedInstruction &instruction,
                const ZydisDecodedOperand &operand,
                uintptr_t instruction_address,
                uintptr_t &address)
            {
                address = 0;
                if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY)
                {
                    return false;
                }

                uint64_t base = 0;
                if (operand.mem.base == ZYDIS_REGISTER_RIP)
                {
                    base = instruction_address + instruction.length;
                }
                else if (operand.mem.base != ZYDIS_REGISTER_NONE)
                {
                    if (!ReadRegister(context, operand.mem.base, base))
                    {
                        return false;
                    }
                }

                uint64_t index = 0;
                if (operand.mem.index != ZYDIS_REGISTER_NONE)
                {
                    if (!ReadRegister(context, operand.mem.index, index))
                    {
                        return false;
                    }
                }

                const uint64_t scale = operand.mem.index == ZYDIS_REGISTER_NONE
                    ? 0
                    : (operand.mem.scale != 0 ? operand.mem.scale : 1);
                const int64_t displacement = operand.mem.disp.size != 0
                    ? static_cast<int64_t>(operand.mem.disp.value)
                    : 0;

                uint64_t effective = base;
                if (scale != 0)
                {
                    effective += index * scale;
                }

                effective = static_cast<uint64_t>(static_cast<int64_t>(effective) + displacement);
                address = static_cast<uintptr_t>(effective);
                return true;
            }

            uint32_t ResolveOperandBits(
                const ZydisDecodedInstruction &instruction,
                const ZydisDecodedOperand *operands,
                uint8_t operand_index)
            {
                const ZydisDecodedOperand &operand = operands[operand_index];
                if (operand.size != 0)
                {
                    return operand.size;
                }

                if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER)
                {
                    return ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, operand.reg.value);
                }

                if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY)
                {
                    return static_cast<uint32_t>(heap_peek::ResolveHeapPeekSize(instruction, operands, operand_index, false)) * 8u;
                }

                return instruction.operand_width;
            }

            bool DecodeFullInstruction(uintptr_t address, ZydisDecodedInstruction &instruction, ZydisDecodedOperand *operands)
            {
                uint8_t bytes[16] = {};
                size_t count = 0;
                while (count < sizeof(bytes) && SafeReadMemoryBytes(address + count, bytes + count, 1))
                {
                    ++count;
                    if (ZYAN_SUCCESS(ZydisDecoderDecodeFull(&GetDecoder(), bytes, count, &instruction, operands)))
                    {
                        return true;
                    }
                }

                return false;
            }

            std::string FormatInstruction(uintptr_t address, const ZydisDecodedInstruction &instruction, const ZydisDecodedOperand *operands)
            {
                char text[256] = {};
                if (!ZYAN_SUCCESS(ZydisFormatterFormatInstruction(
                        &GetFormatter(),
                        &instruction,
                        operands,
                        instruction.operand_count_visible,
                        text,
                        sizeof(text),
                        address,
                        nullptr)))
                {
                    return {};
                }

                return text;
            }

            std::string FormatOperandText(uintptr_t address, const ZydisDecodedInstruction &instruction, const ZydisDecodedOperand &operand)
            {
                char text[128] = {};
                if (!ZYAN_SUCCESS(ZydisFormatterFormatOperand(
                        &GetFormatter(),
                        &instruction,
                        &operand,
                        text,
                        sizeof(text),
                        address,
                        nullptr)))
                {
                    return {};
                }

                return text;
            }

            bool IsAddressComputationInstruction(const ZydisDecodedInstruction &instruction)
            {
                return instruction.mnemonic == ZYDIS_MNEMONIC_LEA;
            }

            bool IsPseudoMemoryInstruction(const ZydisDecodedInstruction &instruction)
            {
                return instruction.mnemonic == ZYDIS_MNEMONIC_NOP;
            }

            std::string FormatInstructionBytes(const uint8_t *bytes, uint8_t size)
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
        }
    }
}
