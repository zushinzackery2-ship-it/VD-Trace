#include "pch.h"
#include "core/extender/VDTraceExtenderAnalyzeInternal.h"

namespace vdtrace
{
    namespace extender
    {
        namespace detail
        {
            bool ReadScalarMemory(
                const std::vector<SimMemoryCell> &overlay,
                uintptr_t address,
                uintptr_t stack_pointer,
                uint32_t &live_private_reads_remaining,
                uint32_t bits,
                uint64_t &value)
            {
                const uint32_t size = (bits + 7u) / 8u;
                if (size == 0 || size > sizeof(uint64_t))
                {
                    return false;
                }

                for (auto it = overlay.rbegin(); it != overlay.rend(); ++it)
                {
                    if (it->address == address && it->size == size)
                    {
                        value = it->value & BitMask(bits);
                        return true;
                    }
                }

                value = 0;
                MEMORY_BASIC_INFORMATION info = {};
                if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &info, sizeof(info)) == 0
                    || info.State != MEM_COMMIT
                    || (info.Protect & PAGE_GUARD) != 0
                    || (info.Protect & PAGE_NOACCESS) != 0)
                {
                    return false;
                }

                if (info.Type == MEM_PRIVATE)
                {
                    if (stack_pointer == 0)
                    {
                        return false;
                    }

                    MEMORY_BASIC_INFORMATION stack_info = {};
                    if (VirtualQuery(reinterpret_cast<LPCVOID>(stack_pointer), &stack_info, sizeof(stack_info)) == 0)
                    {
                        return false;
                    }

                    const uintptr_t region_base = reinterpret_cast<uintptr_t>(
                        info.AllocationBase != nullptr ? info.AllocationBase : info.BaseAddress);
                    const uintptr_t stack_base = reinterpret_cast<uintptr_t>(
                        stack_info.AllocationBase != nullptr ? stack_info.AllocationBase : stack_info.BaseAddress);
                    if (region_base != stack_base)
                    {
                        if (live_private_reads_remaining == 0)
                        {
                            return false;
                        }

                        --live_private_reads_remaining;
                    }
                }

                return SafeReadMemoryBytes(address, &value, size);
            }

            bool TryResolveOperandValue(
                SimContext &context,
                const std::vector<SimMemoryCell> &overlay,
                const ZydisDecodedInstruction &instruction,
                const ZydisDecodedOperand *operands,
                uint8_t operand_index,
                const ZydisDecodedOperand &operand,
                uintptr_t address,
                uint64_t &value)
            {
                switch (operand.type)
                {
                case ZYDIS_OPERAND_TYPE_REGISTER:
                    return ReadRegister(context, operand.reg.value, value);

                case ZYDIS_OPERAND_TYPE_IMMEDIATE:
                    value = operand.imm.is_signed
                        ? SignExtend(static_cast<uint64_t>(operand.imm.value.s), operand.size)
                        : static_cast<uint64_t>(operand.imm.value.u) & BitMask(operand.size);
                    return true;

                case ZYDIS_OPERAND_TYPE_MEMORY:
                {
                    if (operand.mem.type != ZYDIS_MEMOP_TYPE_MEM)
                    {
                        return false;
                    }

                    uintptr_t absolute = 0;
                    if (!TryComputeOperandAddress(context, instruction, operand, address, absolute))
                    {
                        return false;
                    }

                    const uintptr_t stack_pointer = context.regs[SimRsp].known ? context.regs[SimRsp].value : 0;
                    const uint32_t bits = ResolveOperandBits(instruction, operands, operand_index);
                    return ReadScalarMemory(
                        overlay,
                        absolute,
                        stack_pointer,
                        context.live_private_reads_remaining,
                        bits,
                        value);
                }

                default:
                    return false;
                }
            }

            bool QueueMemoryOperand(
                const RecorderQueuedEvent &event,
                const SimContext &context,
                const std::vector<SimMemoryCell> &overlay,
                uintptr_t instruction_address,
                const ZydisDecodedInstruction &instruction,
                const ZydisDecodedOperand *operands,
                uint8_t operand_index,
                const ZydisDecodedOperand &operand,
                const std::string &disasm,
                std::vector<ExtendedMemoryAccess> &accesses)
            {
                if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY || IsPseudoMemoryInstruction(instruction))
                {
                    return false;
                }

                const bool address_only = IsAddressComputationInstruction(instruction);
                if (!address_only && operand.mem.type != ZYDIS_MEMOP_TYPE_MEM)
                {
                    return false;
                }

                const ZyanU8 reads = operand.actions & ZYDIS_OPERAND_ACTION_MASK_READ;
                const ZyanU8 writes = operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE;

                uintptr_t memory_address = 0;
                if (!TryComputeOperandAddress(context, instruction, operand, instruction_address, memory_address))
                {
                    return false;
                }

                const uintptr_t stack_pointer = context.regs[SimRsp].known ? context.regs[SimRsp].value : event.stack_pointer;

                ExtendedMemoryAccess access = {};
                access.sequence = event.sequence;
                access.thread_id = event.thread_id;
                access.kind = event.kind;
                access.block_begin = event.block_begin;
                access.block_end = event.block_end;
                access.instruction = instruction_address;
                access.memory_address = memory_address;
                access.stack_pointer = stack_pointer;
                access.peek_size = heap_peek::ResolveHeapPeekSize(instruction, operands, operand_index, address_only);
                access.instruction_size = instruction.length;
                if (access.instruction_size != 0)
                {
                    SafeReadMemoryBytes(instruction_address, access.instruction_bytes, access.instruction_size);
                }

                access.access_kind = address_only
                    ? ExtendedAccessKind::Address
                    : ExtendedAccessKind::Memory;
                access.origin = address_only
                    ? heap_peek::HeapPeekOrigin::DirectEvent
                    : reads != 0 && writes != 0
                        ? heap_peek::HeapPeekOrigin::BlockExtendReadWrite
                        : writes != 0
                            ? heap_peek::HeapPeekOrigin::BlockExtendWrite
                            : heap_peek::HeapPeekOrigin::BlockExtendRead;
                access.disasm = disasm;
                access.operand_text = FormatOperandText(instruction_address, instruction, operand);

                if (!address_only
                    && reads == 0
                    && writes != 0
                    && operand.size != 0
                    && operand.size <= 64)
                {
                    for (uint8_t source_index = 0; source_index < instruction.operand_count_visible; ++source_index)
                    {
                        if (source_index == operand_index)
                        {
                            continue;
                        }

                        const ZydisDecodedOperand &source_operand = operands[source_index];
                        if ((source_operand.actions & ZYDIS_OPERAND_ACTION_MASK_READ) == 0)
                        {
                            continue;
                        }

                        SimContext preview_context = context;
                        uint64_t value = 0;
                        if (!TryResolveOperandValue(preview_context, overlay, instruction, operands, source_index, source_operand, instruction_address, value))
                        {
                            continue;
                        }

                        const uint32_t bits = ResolveOperandBits(instruction, operands, operand_index);
                        access.has_known_value = true;
                        access.known_value_size = static_cast<uint8_t>((bits + 7u) / 8u);
                        for (uint8_t byte_index = 0; byte_index < access.known_value_size; ++byte_index)
                        {
                            access.known_value_bytes[byte_index] = static_cast<uint8_t>((value >> (byte_index * 8u)) & 0xFFu);
                        }
                        break;
                    }
                }

                accesses.push_back(std::move(access));
                return true;
            }

        }
    }
}
