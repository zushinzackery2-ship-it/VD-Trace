#include "pch.h"
#include "core/runtime/VDTraceInternal.h"
#include "core/hardware/VDTraceHardwareInternal.h"

namespace vdtrace
{
    bool IsHardwareTerminator(EventKind kind)
    {
        return kind == EventKind::Call
            || kind == EventKind::Jump
            || kind == EventKind::ConditionalJump
            || kind == EventKind::Return
            || kind == EventKind::Syscall
            || kind == EventKind::Interrupt;
    }

    bool AnalyzeBasicBlock(uintptr_t entry, BasicBlockInfo &block, std::wstring &error)
    {
        constexpr uint32_t kMaxBlockInstructions = 256;
        constexpr uintptr_t kMaxBlockSpan = 4096;

        error.clear();
        block = {};
        block.entry = entry;

        uintptr_t cursor = entry;
        uintptr_t span = 0;
        while (block.instruction_count < kMaxBlockInstructions && span < kMaxBlockSpan)
        {
            const InstructionDecodeResult decode = DecodeInstruction(cursor);
            if (decode.size == 0)
            {
                error = L"解析基本块失败。";
                return false;
            }

            block.instruction_count++;
            span += decode.size;
            if (IsHardwareTerminator(decode.kind))
            {
                block.valid = true;
                block.tail = cursor;
                block.fallthrough = cursor + decode.size;
                block.tail_decode = decode;
                block.emits_edge = true;
                return true;
            }

            cursor += decode.size;
        }

        block.valid = true;
        block.truncated = true;
        block.tail = cursor;
        block.fallthrough = cursor;
        block.emits_edge = false;
        return true;
    }

    bool PrepareHardwareFlowStart(Session::Impl &impl, uintptr_t entry, std::wstring &error)
    {
        return ProgramHardwareObservationImpl(impl, entry, nullptr, error);
    }

    bool ProgramHardwareObservation(Session::Impl &impl, uintptr_t entry, CONTEXT *context, std::wstring &error)
    {
        return ProgramHardwareObservationImpl(impl, entry, context, error);
    }
}
