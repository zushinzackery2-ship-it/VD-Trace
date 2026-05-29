#include "pch.h"
#include "core/extender/VDTraceExtenderAnalyzeInternal.h"

namespace vdtrace
{
    namespace extender
    {
        namespace detail
        {
            void ApplyInstructionEffects(
                SimContext &context,
                std::vector<SimMemoryCell> &overlay,
                const ZydisDecodedInstruction &instruction,
                const ZydisDecodedOperand *operands,
                uintptr_t instruction_address)
            {
                const auto mark_dest_unknown = [&]()
                {
                    for (uint8_t index = 0; index < instruction.operand_count_visible; ++index)
                    {
                        if ((operands[index].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0)
                        {
                            MarkOperandRegisterUnknown(context, operands[index]);
                        }
                    }
                };

                const auto assign_binary = [&](auto fn)
                {
                    if (instruction.operand_count_visible < 2 || operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER)
                    {
                        mark_dest_unknown();
                        return;
                    }

                    uint64_t lhs = 0;
                    uint64_t rhs = 0;
                    if (!TryResolveOperandValue(context, overlay, instruction, operands, 0, operands[0], instruction_address, lhs)
                        || !TryResolveOperandValue(context, overlay, instruction, operands, 1, operands[1], instruction_address, rhs))
                    {
                        MarkOperandRegisterUnknown(context, operands[0]);
                        return;
                    }

                    WriteRegister(context, operands[0].reg.value, fn(lhs, rhs));
                };

                const auto assign_unary = [&](auto fn)
                {
                    if (instruction.operand_count_visible < 1 || operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER)
                    {
                        mark_dest_unknown();
                        return;
                    }

                    uint64_t value = 0;
                    if (!TryResolveOperandValue(context, overlay, instruction, operands, 0, operands[0], instruction_address, value))
                    {
                        MarkOperandRegisterUnknown(context, operands[0]);
                        return;
                    }

                    WriteRegister(context, operands[0].reg.value, fn(value));
                };

                const auto assign_imul = [&]()
                {
                    if (instruction.operand_count_visible < 2 || operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER)
                    {
                        mark_dest_unknown();
                        return;
                    }

                    const uint32_t width = operands[0].size != 0
                        ? operands[0].size
                        : ZydisRegisterGetWidth(ZYDIS_MACHINE_MODE_LONG_64, operands[0].reg.value);

                    uint64_t lhs = 0;
                    uint64_t rhs = 0;
                    if (instruction.operand_count_visible >= 3)
                    {
                        if (!TryResolveOperandValue(context, overlay, instruction, operands, 1, operands[1], instruction_address, lhs)
                            || !TryResolveOperandValue(context, overlay, instruction, operands, 2, operands[2], instruction_address, rhs))
                        {
                            MarkOperandRegisterUnknown(context, operands[0]);
                            return;
                        }
                    }
                    else
                    {
                        if (!TryResolveOperandValue(context, overlay, instruction, operands, 0, operands[0], instruction_address, lhs)
                            || !TryResolveOperandValue(context, overlay, instruction, operands, 1, operands[1], instruction_address, rhs))
                        {
                            MarkOperandRegisterUnknown(context, operands[0]);
                            return;
                        }
                    }

                    const int64_t lhs_signed = static_cast<int64_t>(SignExtend(lhs, width));
                    const int64_t rhs_signed = static_cast<int64_t>(SignExtend(rhs, width));
                    WriteRegister(context, operands[0].reg.value, static_cast<uint64_t>(lhs_signed * rhs_signed));
                };

                if (instruction.mnemonic == ZYDIS_MNEMONIC_MOV)
                {
                    if (instruction.operand_count_visible >= 2 && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER)
                    {
                        uint64_t value = 0;
                        if (TryResolveOperandValue(context, overlay, instruction, operands, 1, operands[1], instruction_address, value))
                        {
                            WriteRegister(context, operands[0].reg.value, value);
                            return;
                        }
                    }

                    if (instruction.operand_count_visible >= 2
                        && operands[0].type == ZYDIS_OPERAND_TYPE_MEMORY
                        && operands[0].mem.type == ZYDIS_MEMOP_TYPE_MEM)
                    {
                        uint64_t value = 0;
                        if (TryResolveOperandValue(context, overlay, instruction, operands, 1, operands[1], instruction_address, value))
                        {
                            uintptr_t absolute = 0;
                            if (TryComputeOperandAddress(context, instruction, operands[0], instruction_address, absolute))
                            {
                                const uint32_t size = (ResolveOperandBits(instruction, operands, 0) + 7u) / 8u;
                                if (size != 0 && size <= sizeof(uint64_t))
                                {
                                    overlay.push_back({absolute, value, size});
                                }
                                return;
                            }
                        }
                    }

                    mark_dest_unknown();
                    return;
                }

                if (instruction.mnemonic == ZYDIS_MNEMONIC_LEA)
                {
                    if (instruction.operand_count_visible >= 2 && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER)
                    {
                        uintptr_t absolute = 0;
                        if (TryComputeOperandAddress(context, instruction, operands[1], instruction_address, absolute))
                        {
                            WriteRegister(context, operands[0].reg.value, absolute);
                            return;
                        }
                    }

                    mark_dest_unknown();
                    return;
                }

                if (instruction.mnemonic == ZYDIS_MNEMONIC_MOVZX
                    || instruction.mnemonic == ZYDIS_MNEMONIC_MOVSX
                    || instruction.mnemonic == ZYDIS_MNEMONIC_MOVSXD)
                {
                    if (instruction.operand_count_visible >= 2 && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER)
                    {
                        uint64_t value = 0;
                        if (TryResolveOperandValue(context, overlay, instruction, operands, 1, operands[1], instruction_address, value))
                        {
                            const uint32_t source_bits = ResolveOperandBits(instruction, operands, 1);
                            const uint64_t resolved = instruction.mnemonic == ZYDIS_MNEMONIC_MOVZX
                                ? value & BitMask(source_bits)
                                : SignExtend(value, source_bits);
                            WriteRegister(context, operands[0].reg.value, resolved);
                            return;
                        }
                    }

                    mark_dest_unknown();
                    return;
                }

                switch (instruction.mnemonic)
                {
                case ZYDIS_MNEMONIC_IMUL:
                    assign_imul();
                    return;
                case ZYDIS_MNEMONIC_ADD:
                    assign_binary([](uint64_t lhs, uint64_t rhs) { return lhs + rhs; });
                    return;
                case ZYDIS_MNEMONIC_SUB:
                    assign_binary([](uint64_t lhs, uint64_t rhs) { return lhs - rhs; });
                    return;
                case ZYDIS_MNEMONIC_INC:
                    assign_unary([](uint64_t value) { return value + 1; });
                    return;
                case ZYDIS_MNEMONIC_DEC:
                    assign_unary([](uint64_t value) { return value - 1; });
                    return;
                case ZYDIS_MNEMONIC_XOR:
                    if (instruction.operand_count_visible >= 2
                        && operands[0].type == ZYDIS_OPERAND_TYPE_REGISTER
                        && operands[1].type == ZYDIS_OPERAND_TYPE_REGISTER
                        && operands[0].reg.value == operands[1].reg.value)
                    {
                        WriteRegister(context, operands[0].reg.value, 0);
                        return;
                    }
                    assign_binary([](uint64_t lhs, uint64_t rhs) { return lhs ^ rhs; });
                    return;
                case ZYDIS_MNEMONIC_AND:
                    assign_binary([](uint64_t lhs, uint64_t rhs) { return lhs & rhs; });
                    return;
                case ZYDIS_MNEMONIC_OR:
                    assign_binary([](uint64_t lhs, uint64_t rhs) { return lhs | rhs; });
                    return;
                case ZYDIS_MNEMONIC_SHL:
                    assign_binary([](uint64_t lhs, uint64_t rhs) { return lhs << (rhs & 0x3F); });
                    return;
                case ZYDIS_MNEMONIC_SHR:
                    assign_binary([](uint64_t lhs, uint64_t rhs) { return lhs >> (rhs & 0x3F); });
                    return;
                case ZYDIS_MNEMONIC_SAR:
                    assign_binary([](uint64_t lhs, uint64_t rhs) { return static_cast<uint64_t>(static_cast<int64_t>(lhs) >> (rhs & 0x3F)); });
                    return;
                default:
                    mark_dest_unknown();
                    return;
                }
            }
        }
    }
}
