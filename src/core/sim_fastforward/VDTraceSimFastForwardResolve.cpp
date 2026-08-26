#include "pch.h"
#include "core/sim_fastforward/VDTraceSimFastForwardInternal.h"

namespace vdtrace::sim_fastforward::detail
{
    namespace
    {
        // A jump target is only skippable when it lands inside a tracked module that is
        // handled by the plain DR edge backend. Anything that would normally divert into
        // a TF filter window, a system module, or untracked code must fault for real so
        // the existing region/filter logic runs.
        bool TargetIsPlainTrackedModule(Session::Impl &impl, uintptr_t target)
        {
            if (target == 0 || FindModuleRange(impl.module_ranges, target) == nullptr)
            {
                return false;
            }

            bool has_mode = false;
            bool is_system = false;
            const DepthFilterExecutionMode mode = ResolveExecutionModeForAddress(impl, target, has_mode, is_system);
            if (is_system)
            {
                return false;
            }

            return !has_mode || mode != DepthFilterExecutionMode::TrapFlag;
        }

        // Replays the modeled effects of the straight-line body so a register-indirect
        // tail can be resolved from a purely register-derived value. Any genuine memory
        // read (LEA excluded) or undecodable instruction taints the window.
        void ScanInteriorForRegisters(WalkState &state, uintptr_t entry, uintptr_t tail)
        {
            uintptr_t cursor = entry;
            uint32_t count = 0;
            while (cursor < tail && count++ < extender::detail::kMaxExtendedInstructions)
            {
                ZydisDecodedInstruction instruction = {};
                ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
                if (!extender::detail::DecodeFullInstruction(cursor, instruction, operands))
                {
                    state.registers_pure = false;
                    return;
                }

                for (uint8_t index = 0; index < instruction.operand_count_visible; ++index)
                {
                    const ZydisDecodedOperand &operand = operands[index];
                    if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY
                        && instruction.mnemonic != ZYDIS_MNEMONIC_LEA
                        && (operand.actions & ZYDIS_OPERAND_ACTION_MASK_READ) != 0)
                    {
                        state.registers_pure = false;
                    }
                }

                extender::detail::ApplyInstructionEffects(state.sim, state.overlay, instruction, operands, cursor);
                cursor += instruction.length;
                state.sim.regs[extender::detail::SimRip] = {cursor, true};
            }
        }

        bool TryResolveIndirectTarget(WalkState &state, const BasicBlockInfo &block, uintptr_t &target)
        {
            target = 0;
            if (!state.track_registers || !state.registers_pure)
            {
                return false;
            }

            ZydisDecodedInstruction instruction = {};
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
            if (!extender::detail::DecodeFullInstruction(block.tail, instruction, operands))
            {
                return false;
            }

            // Only a register-operand jump is provably equal to the CPU result here;
            // memory-indirect jumps stay on the baseline fault path.
            if (instruction.operand_count_visible < 1 || operands[0].type != ZYDIS_OPERAND_TYPE_REGISTER)
            {
                return false;
            }

            uint64_t value = 0;
            if (!extender::detail::ReadRegister(state.sim, operands[0].reg.value, value))
            {
                return false;
            }

            target = static_cast<uintptr_t>(value);
            return true;
        }
    }

    bool TryPlanSkippableJump(Session::Impl &impl, uintptr_t entry, WalkState &state, PredictedJump &plan)
    {
        std::wstring error;
        BasicBlockInfo block = {};
        if (!AnalyzeBasicBlock(entry, block, error)
            || !block.valid
            || block.truncated
            || !block.emits_edge
            || block.tail_decode.kind != EventKind::Jump)
        {
            return false;
        }

        // Only skip jumps whose source lives inside a tracked module - the predicted
        // edge has to be one the trace would otherwise record.
        if (FindModuleRange(impl.module_ranges, block.tail) == nullptr)
        {
            return false;
        }

        if (HasValueProbeInRange(impl, entry + 1, block.fallthrough))
        {
            return false;
        }

        if (state.track_registers)
        {
            ScanInteriorForRegisters(state, entry, block.tail);
        }

        uintptr_t target = 0;
        if (block.tail_decode.has_target)
        {
            target = block.tail_decode.target;
        }
        else if (!TryResolveIndirectTarget(state, block, target))
        {
            return false;
        }

        if (!TargetIsPlainTrackedModule(impl, target))
        {
            return false;
        }

        plan.block_entry = block.entry;
        plan.tail = block.tail;
        plan.fallthrough = block.fallthrough;
        plan.target = target;
        plan.tail_decode = block.tail_decode;
        return true;
    }
}
