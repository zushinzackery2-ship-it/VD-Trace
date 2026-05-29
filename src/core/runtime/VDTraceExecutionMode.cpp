#include "pch.h"
#include "core/runtime/VDTraceInternal.h"

namespace vdtrace
{
    namespace
    {
        bool ResolveTrapWindowRange(
            const Session::Impl &impl,
            uintptr_t address,
            uintptr_t &region_base,
            size_t &region_size)
        {
            region_base = 0;
            region_size = 0;

            if (address == 0)
            {
                return false;
            }

            if (const auto *rule = FindDepthFilterModuleRule(impl.depth_filters, address); rule != nullptr)
            {
                region_base = rule->range.base;
                region_size = rule->range.size;
                return region_size != 0;
            }

            if (const auto *tracked = FindModuleRange(impl.module_ranges, address); tracked != nullptr)
            {
                region_base = tracked->base;
                region_size = tracked->size;
                return region_size != 0;
            }

            AddressLabel label = {};
            if (ResolveAddressLabel(address, label) && label.valid && label.module_size != 0)
            {
                region_base = label.module_base;
                region_size = label.module_size;
                return true;
            }

            MEMORY_BASIC_INFORMATION mbi = {};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0)
            {
                return false;
            }

            region_base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            region_size = mbi.RegionSize;
            return region_size != 0;
        }

        bool TryActivateFilterTrapSessionImpl(
            Session::Impl &impl,
            uintptr_t instruction,
            CONTEXT *context,
            bool &activated,
            std::wstring &error)
        {
            activated = false;
            error.clear();

            if (!impl.options.control_flow_only || impl.active_probe.active)
            {
                return true;
            }

            bool has_tf_mode = false;
            bool is_system_target = false;
            const DepthFilterExecutionMode execution_mode = ResolveExecutionModeForAddress(
                impl,
                instruction,
                has_tf_mode,
                is_system_target);
            if (!has_tf_mode || is_system_target || execution_mode != DepthFilterExecutionMode::TrapFlag)
            {
                return true;
            }

            uintptr_t region_base = 0;
            size_t region_size = 0;
            if (!ResolveTrapWindowRange(impl, instruction, region_base, region_size))
            {
                error = L"过滤器 TF 区域范围解析失败。";
                return false;
            }

            ActiveProbeSession session = {};
            session.active = true;
            session.restore_hardware_backend = true;
            session.mode = ProbeMode::FilterTrace;
            session.exit_kind = ProbeExitKind::LeaveRegion;
            session.start_call_depth = impl.current_call_depth.load();
            session.pending_entry = instruction;
            session.region_base = region_base;
            session.region_size = region_size;

            if (!ActivateTrapWindow(impl, session, instruction, context, error))
            {
                return false;
            }

            activated = true;
            return true;
        }

        bool TryArmReturnOnlyResume(
            Session::Impl &impl,
            uintptr_t current_rip,
            CONTEXT *context,
            bool &armed,
            std::wstring &error)
        {
            armed = false;
            error.clear();

            if (context == nullptr
                || impl.call_return_stack.empty()
                || impl.trap_suppression_stack.empty()
                || impl.trap_suppression_stack.back() == 0)
            {
                return true;
            }

            const uintptr_t resume_address = impl.call_return_stack.back();
            if (resume_address == 0 || current_rip == resume_address)
            {
                return true;
            }

            impl.pending_block = {};
            ClearPendingResolution(impl);
            impl.pending_async_handoff_target = 0;
            impl.pending_async_handoff_event = {};
            impl.pending_async_handoff_valid = false;
            impl.observed_addresses[0] = resume_address;
            impl.observed_addresses[1] = 0;
            impl.observed_addresses[2] = 0;
            impl.observed_addresses[3] = 0;
            impl.observed_count = 1;
            impl.observation_state = ObservationState::WaitingForHotReturn;
            impl.hot_bypass_resume = 0;
            impl.hot_bypass_return = resume_address;
            impl.hot_bypass_call_depth = impl.current_call_depth.load();
            impl.hot_bypass_root_stop = false;
            impl.last_rip = 0;
            impl.last_rip_valid = false;
            ApplyHardwareContextObservations(*context, impl.observed_addresses, impl.observed_count, false);
            armed = true;
            return true;
        }
    }

    bool ActivateTrapWindow(
        Session::Impl &impl,
        const ActiveProbeSession &session,
        uintptr_t entry,
        CONTEXT *context,
        std::wstring &error)
    {
        error.clear();

        impl.active_probe = session;
        impl.backend_mode = BackendMode::TrapFlagContext;
        impl.observation_state = ObservationState::Idle;
        impl.pending_block = {};
        ClearPendingResolution(impl);
        impl.pending_async_handoff_target = 0;
        impl.pending_async_handoff_event = {};
        impl.pending_async_handoff_valid = false;
        std::memset(impl.observed_addresses, 0, sizeof(impl.observed_addresses));
        impl.observed_count = 0;
        impl.last_rip = entry;
        impl.last_rip_valid = true;

        if (context != nullptr)
        {
            uintptr_t empty[4] = {};
            ApplyHardwareContextObservations(*context, empty, 0, true);
            return true;
        }

        if (impl.thread_handle == nullptr)
        {
            error = L"切换局部 TF 失败：缺少线程句柄。";
            return false;
        }

        return ArmSingleStep(impl.thread_handle, error);
    }

    bool TryActivateFilterTrapSession(
        Session::Impl &impl,
        uintptr_t instruction,
        CONTEXT *context,
        bool &activated,
        std::wstring &error)
    {
        return TryActivateFilterTrapSessionImpl(impl, instruction, context, activated, error);
    }

    bool StartRegionAwareHardwareFlow(Session::Impl &impl, uintptr_t entry, CONTEXT *context, std::wstring &error)
    {
        error.clear();

        bool activated = false;
        if (!TryActivateFilterTrapSessionImpl(impl, entry, context, activated, error))
        {
            return false;
        }

        if (activated)
        {
            return true;
        }

        std::wstring plan_error;
        const bool prepared = context != nullptr
            ? ProgramHardwareObservation(impl, entry, context, plan_error)
            : PrepareHardwareFlowStart(impl, entry, plan_error);
        if (!prepared)
        {
            if (!BeginHardwareLinearScan(impl, entry, context, error))
            {
                if (error.empty())
                {
                    error = plan_error;
                }
                return false;
            }

            return true;
        }

        if (context != nullptr)
        {
            return true;
        }

        return ArmHardwareExecution(impl.thread_handle, impl.observed_addresses, impl.observed_count, error);
    }

    bool RestoreHardwareFlowAfterTrapWindow(Session::Impl &impl, uintptr_t current_rip, CONTEXT *context, std::wstring &error)
    {
        error.clear();
        impl.backend_mode = BackendMode::HardwareFlow;

        bool armed_return_wait = false;
        if (!TryArmReturnOnlyResume(impl, current_rip, context, armed_return_wait, error))
        {
            return false;
        }

        ResetActiveProbeSession(impl);
        impl.trap_suppression_stack.clear();
        if (armed_return_wait)
        {
            return true;
        }

        if (context == nullptr)
        {
            error = L"恢复硬件流追踪失败：缺少线程上下文。";
            return false;
        }

        return StartRegionAwareHardwareFlow(impl, current_rip, context, error);
    }
}
