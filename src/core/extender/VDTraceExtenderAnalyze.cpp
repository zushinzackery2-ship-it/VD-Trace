#include "pch.h"
#include "core/extender/VDTraceExtenderAnalyzeInternal.h"

namespace vdtrace
{
    namespace extender
    {
        bool AnalyzeBlockMemoryAccesses(
            const RecorderQueuedEvent &event,
            const ThreadContextSnapshot &entry_context,
            std::vector<ExtendedMemoryAccess> &accesses)
        {
            accesses.clear();
            if (!entry_context.valid
                || entry_context.rip != event.block_begin
                || event.block_begin == 0
                || event.block_end <= event.block_begin
                || event.block_end - event.block_begin > detail::kMaxExtendedBlockSpan)
            {
                return false;
            }

            detail::SimContext context = {};
            detail::InitializeContext(entry_context, context);
            std::vector<detail::SimMemoryCell> overlay;

            uintptr_t cursor = event.block_begin;
            uint32_t count = 0;
            while (cursor < event.block_end && count++ < detail::kMaxExtendedInstructions)
            {
                ZydisDecodedInstruction instruction = {};
                ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
                if (!detail::DecodeFullInstruction(cursor, instruction, operands))
                {
                    return false;
                }

                const std::string disasm = detail::FormatInstruction(cursor, instruction, operands);
                for (uint8_t index = 0; index < instruction.operand_count_visible; ++index)
                {
                    detail::QueueMemoryOperand(event, context, overlay, cursor, instruction, operands, index, operands[index], disasm, accesses);
                }

                detail::ApplyInstructionEffects(context, overlay, instruction, operands, cursor);
                cursor += instruction.length;
                context.regs[detail::SimRip] = {cursor, true};
            }

            return cursor == event.block_end;
        }

        std::string FormatExtendedMemoryAccessLine(const ExtendedMemoryAccess &access)
        {
            std::ostringstream out;
            if (access.access_kind == ExtendedAccessKind::Address)
            {
                out << "   [extend.addr] seq=" << access.sequence
                    << " rip=0x" << std::hex << access.instruction
                    << " addr=0x" << access.memory_address
                    << " kind=lea"
                    << "\n";
            }
            else
            {
                const char *mode = access.origin == heap_peek::HeapPeekOrigin::BlockExtendRead
                    ? "read"
                    : access.origin == heap_peek::HeapPeekOrigin::BlockExtendWrite
                        ? "write"
                        : "rw";
                out << "   [extend.mem] seq=" << access.sequence
                    << " rip=0x" << std::hex << access.instruction
                    << " addr=0x" << access.memory_address
                    << " size=" << std::dec << static_cast<unsigned>(access.peek_size)
                    << " mode=" << mode
                    << "\n";
            }

            if (access.instruction_size != 0)
            {
                out << "       0x" << std::hex << access.instruction
                    << "  " << detail::FormatInstructionBytes(access.instruction_bytes, access.instruction_size)
                    << "\n";
            }

            out << "       " << access.disasm;
            return out.str();
        }
    }
}
