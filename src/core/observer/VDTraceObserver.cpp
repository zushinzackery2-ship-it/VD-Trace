#include "pch.h"
#include "core/runtime/VDTraceInternal.h"

namespace vdtrace
{
    namespace
    {
        void CaptureProbeModuleInfo(const Session::Impl &impl, uintptr_t instruction, StepEvent &event)
        {
            if (const auto *range = FindModuleRange(impl.module_ranges, instruction); range != nullptr)
            {
                event.inside_module = true;
                event.module_base = range->base;
                event.module_size = range->size;
                event.relative_instruction = instruction - range->base;
                if (!impl.lightweight_module_capture)
                {
                    event.module_name = range->name;
                }
            }
        }

        void FillProbeWatchCapture(const ActiveProbeWatchState &watch, ProbeCapture &capture)
        {
            capture = {};
            capture.valid = watch.valid;
            capture.value = watch.address;
            capture.size = watch.size;
            capture.has_bytes = watch.valid && watch.size != 0;
            std::memcpy(capture.label, watch.label, sizeof(capture.label));
            std::memcpy(capture.bytes, watch.bytes, sizeof(capture.bytes));
        }

        bool TrySnapshotWatch(
            ActiveProbeWatchState &watch,
            ProbeCapture *capture)
        {
            uint8_t current_bytes[kEnhancedSampleMaxBytes] = {};
            const bool readable = watch.address != 0
                && watch.size != 0
                && SafeReadMemoryBytes(watch.address, current_bytes, watch.size);
            if (!readable)
            {
                return false;
            }

            const bool changed = !watch.valid || std::memcmp(watch.bytes, current_bytes, watch.size) != 0;
            watch.valid = true;
            std::memcpy(watch.bytes, current_bytes, sizeof(watch.bytes));
            if (!changed || capture == nullptr)
            {
                return changed;
            }

            FillProbeWatchCapture(watch, *capture);
            return true;
        }

        bool ShouldExitProbeByReturn(const ActiveProbeSession &session, EventKind kind, uint32_t call_depth)
        {
            if (kind != EventKind::Return)
            {
                return false;
            }

            return call_depth == session.start_call_depth;
        }

        bool ShouldExitProbeByLeave(const ActiveProbeSession &session, uintptr_t current_rip)
        {
            if (session.region_size == 0)
            {
                return false;
            }

            return current_rip < session.region_base
                || current_rip >= (session.region_base + session.region_size);
        }

        bool RestoreHardwareFlowFromProbe(Session::Impl &impl, uintptr_t current_rip, CONTEXT *context, std::wstring &error)
        {
            return RestoreHardwareFlowAfterTrapWindow(impl, current_rip, context, error);
        }
    }

    void ResetActiveProbeSession(Session::Impl &impl)
    {
        impl.active_probe = {};
    }

    bool TryActivateLocalProbeSession(
        Session::Impl &impl,
        uintptr_t instruction,
        CONTEXT *context,
        bool switch_from_hardware_flow,
        bool &activated,
        std::wstring &error)
    {
        activated = false;
        error.clear();

        if (context == nullptr || impl.active_probe.active || impl.value_probes.empty())
        {
            return true;
        }

        const auto it = std::lower_bound(
            impl.value_probes.begin(),
            impl.value_probes.end(),
            instruction,
            [](const ResolvedValueProbe &probe, uintptr_t value)
            {
                return probe.address < value;
            });
        if (it == impl.value_probes.end() || it->address != instruction || it->mode == ProbeMode::Capture)
        {
            return true;
        }

        ActiveProbeSession session = {};
        session.active = true;
        session.restore_hardware_backend = switch_from_hardware_flow;
        session.mode = it->mode;
        session.exit_kind = it->exit_kind;
        session.remaining_steps = it->step_limit;
        session.start_call_depth = impl.current_call_depth.load();
        session.pending_entry = instruction;

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(instruction), &mbi, sizeof(mbi)) != 0)
        {
            session.region_base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            session.region_size = mbi.RegionSize;
        }
        else
        {
            session.region_base = instruction;
            session.region_size = 1;
        }

        if (it->mode == ProbeMode::WriteTrace)
        {
            for (uint8_t index = 0; index < it->watch_count && index < kProbeCaptureMaxCount; ++index)
            {
                session.watches[index].address = it->watches[index].address;
                session.watches[index].size = it->watches[index].size;
                std::memcpy(session.watches[index].label, it->watches[index].label, sizeof(session.watches[index].label));
                TrySnapshotWatch(session.watches[index], nullptr);
                session.watch_count++;
            }
        }

        impl.active_probe = session;
        activated = true;

        if (!switch_from_hardware_flow)
        {
            return true;
        }

        return ActivateTrapWindow(impl, session, instruction, context, error);
    }

    bool HandleActiveProbeSingleStep(
        Session::Impl &impl,
        uintptr_t executed_rip,
        uintptr_t current_rip,
        const CONTEXT *context,
        const InstructionDecodeResult &decode_result,
        uint32_t call_depth,
        uintptr_t actual_target,
        bool has_actual_target,
        bool &restored_hardware_flow,
        std::wstring &error)
    {
        restored_hardware_flow = false;
        error.clear();

        ActiveProbeSession &session = impl.active_probe;
        if (!session.active || context == nullptr)
        {
            return true;
        }

        if (session.pending_entry != 0)
        {
            if (executed_rip != session.pending_entry)
            {
                return true;
            }

            session.pending_entry = 0;
        }

        StepEvent event = {};
        bool should_emit = session.mode == ProbeMode::SingleStep;
        if (session.mode == ProbeMode::WriteTrace)
        {
            for (uint8_t index = 0; index < session.watch_count && index < kProbeCaptureMaxCount; ++index)
            {
                ProbeCapture capture = {};
                if (!TrySnapshotWatch(session.watches[index], &capture))
                {
                    continue;
                }

                if (capture.valid)
                {
                    event.probe_captures[event.probe_capture_count++] = capture;
                    should_emit = true;
                }
            }
        }

        if (should_emit)
        {
            event.sequence = impl.event_count.fetch_add(1) + 1;
            event.thread_id = GetCurrentThreadId();
            event.instruction = executed_rip;
            event.stack_pointer = static_cast<uintptr_t>(context->Rsp);
            event.block_begin = executed_rip;
            event.block_end = executed_rip + decode_result.size;
            event.call_depth = call_depth;
            event.kind = EventKind::Probe;
            event.instruction_size = decode_result.size;
            std::memcpy(event.instruction_bytes, decode_result.bytes, sizeof(event.instruction_bytes));
            CaptureThreadContext(*context, event.thread_context);
            if (has_actual_target)
            {
                event.has_target = true;
                event.target = actual_target;
            }
            if (decode_result.kind == EventKind::Return)
            {
                event.has_return_value = true;
                event.return_value = static_cast<uintptr_t>(context->Rax);
            }
            CaptureProbeModuleInfo(impl, executed_rip, event);

            if (impl.options.callback != nullptr)
            {
                impl.options.callback(event, impl.options.callback_context);
            }
        }

        const bool has_step_limit = session.mode == ProbeMode::SingleStep
            || session.mode == ProbeMode::WriteTrace;
        if (has_step_limit && session.remaining_steps != 0)
        {
            session.remaining_steps--;
        }

        bool should_exit = has_step_limit && session.remaining_steps == 0;
        if (!should_exit)
        {
            switch (session.exit_kind)
            {
            case ProbeExitKind::Return:
                should_exit = ShouldExitProbeByReturn(session, decode_result.kind, call_depth);
                break;

            case ProbeExitKind::LeaveRegion:
                should_exit = ShouldExitProbeByLeave(session, current_rip);
                break;

            case ProbeExitKind::ReturnOrLeaveRegion:
                should_exit = ShouldExitProbeByReturn(session, decode_result.kind, call_depth)
                    || ShouldExitProbeByLeave(session, current_rip);
                break;
            }
        }

        if (!should_exit)
        {
            return true;
        }

        if (!session.restore_hardware_backend)
        {
            ResetActiveProbeSession(impl);
            return true;
        }

        if (!RestoreHardwareFlowFromProbe(impl, current_rip, const_cast<CONTEXT *>(context), error))
        {
            SnapshotExecutionSummary(impl);
            impl.running.store(false);
            impl.observation_state = ObservationState::Idle;
            return false;
        }

        restored_hardware_flow = true;
        return true;
    }
}
