#include "pch.h"
#include "VDTraceInternal.h"

namespace vdtrace
{
    namespace
    {
        const wchar_t *ObservationStateName(ObservationState state)
        {
            switch (state)
            {
            case ObservationState::Idle:
                return L"idle";
            case ObservationState::WaitingForDestination:
                return L"dest";
            case ObservationState::WaitingForTail:
                return L"tail";
            case ObservationState::WaitingForSingleStep:
                return L"single-step";
            case ObservationState::LinearScan:
                return L"linear-scan";
            case ObservationState::WaitingForHotReturn:
                return L"hot-bypass";
            default:
                return L"unknown";
            }
        }

        const wchar_t *ExecutionRegionName(const Session::Impl &impl, uintptr_t address)
        {
            switch (ResolveExecutionAddress(impl, address).kind)
            {
            case ExecutionAddressKind::TrackedModule:
                return L"module";
            case ExecutionAddressKind::AnonymousExecutable:
                return L"anonymous";
            case ExecutionAddressKind::OutsideImage:
            case ExecutionAddressKind::SystemModule:
                return L"outside";
            case ExecutionAddressKind::Unknown:
            default:
                return L"unknown";
            }
        }

        void AppendDepthValue(std::wostringstream &out, uint32_t value)
        {
            if (value == kUnlimitedCallDepth)
            {
                out << L"all";
                return;
            }

            if (value == 0)
            {
                out << L"single";
                return;
            }

            out << value;
        }

        uintptr_t ResolveStatusAddress(const Session::Impl &impl)
        {
            if (impl.last_rip_valid && impl.last_rip != 0)
            {
                return impl.last_rip;
            }

            if (impl.pending_block.valid && impl.pending_block.entry != 0)
            {
                return impl.pending_block.entry;
            }

            if (impl.observed_count != 0)
            {
                return impl.observed_addresses[0];
            }

            return 0;
        }

        void AppendObservedAddresses(std::wostringstream &out, const Session::Impl &impl)
        {
            if (impl.observed_count == 0)
            {
                return;
            }

            out << L" watch=[";
            for (uint32_t index = 0; index < impl.observed_count; ++index)
            {
                if (index != 0)
                {
                    out << L",";
                }

                out << L"0x" << std::hex << impl.observed_addresses[index] << std::dec;
            }
            out << L"]";
        }
    }

    Session::Session()
        : impl_(std::make_unique<Impl>())
    {
    }

    Session::~Session()
    {
        std::wstring ignored_error;
        impl_->Stop(ignored_error);
        if (impl_->veh_handle != nullptr)
        {
            RemoveVectoredExceptionHandler(impl_->veh_handle);
            impl_->veh_handle = nullptr;
        }
    }

    bool Session::Configure(const Options &options, std::wstring &error)
    {
        return impl_->Configure(options, error);
    }

    bool Session::Start(std::wstring &error)
    {
        return impl_->Start(error);
    }

    bool Session::Stop(std::wstring &error)
    {
        return impl_->Stop(error);
    }

    bool Session::IsConfigured() const
    {
        return impl_->IsConfigured();
    }

    bool Session::IsRunning() const
    {
        return impl_->IsRunning();
    }

    uint64_t Session::EventCount() const
    {
        return impl_->EventCount();
    }

    std::wstring Session::DescribeState() const
    {
        return impl_->DescribeState();
    }

    std::vector<ModuleRange> Session::ModuleRanges() const
    {
        return impl_->ModuleRanges();
    }

    bool Session::Impl::IsConfigured() const
    {
        return configured.load();
    }

    bool Session::Impl::IsRunning() const
    {
        return running.load();
    }

    uint64_t Session::Impl::EventCount() const
    {
        return running.load() ? event_count.load() : retained_event_count.load();
    }

    std::wstring Session::Impl::DescribeState() const
    {
        std::lock_guard<std::mutex> lock(state_lock);

        std::wostringstream out;
        out << L"configured=" << (configured.load() ? 1 : 0)
            << L" running=" << (running.load() ? 1 : 0)
            << L" backend="
            << (backend_mode == BackendMode::TrapFlagContext ? L"tf" : L"dr")
            << L" thread_id=" << options.thread_id
            << L" auto_thread=" << (options.auto_select_thread ? 1 : 0)
            << L" block_main=" << (options.block_main_thread ? 1 : 0)
            << L" focus=" << (options.queue_trigger_threads ? L"queue" : L"single")
            << L" active_thread=" << active_thread_id.load()
            << L" trigger=";
        if (resolved_trigger_address == 0)
        {
            out << L"off";
        }
        else
        {
            out << L"0x" << std::hex << resolved_trigger_address << std::dec;
            if (waiting_for_trigger)
            {
                out << L"(waiting)";
            }
        }
        if (options.auto_select_thread && waiting_for_trigger && active_thread_id.load() == 0)
        {
            out << L" capture=thread";
        }
        out << L" root_stop=" << (options.stop_on_root_return ? 1 : 0)
            << L" handoff=" << (options.async_thread_handoff ? 1 : 0)
            << L" sample=" << (options.enhanced_sampling ? 1 : 0)
            << L" probes=" << value_probes.size()
            << L" event_mode=" << (options.control_flow_only ? L"flow" : L"full")
            << L" call_limit=";
        AppendDepthValue(out, options.max_call_depth);
        out << L" depth_module_rules=" << depth_filters.module_rules.size();
        if (depth_filters.has_outside_module_depth)
        {
            out << L" depth_outside=";
            AppendDepthValue(out, depth_filters.outside_module_depth);
        }
        if (depth_filters.has_anonymous_exec_depth)
        {
            out << L" depth_anon=";
            AppendDepthValue(out, depth_filters.anonymous_exec_depth);
        }
        const uint32_t depth = running.load() ? current_call_depth.load() : retained_call_depth.load();
        const uint64_t steps = running.load() ? step_count.load() : retained_step_count.load();
        const uint64_t events = running.load() ? event_count.load() : retained_event_count.load();
        const uint64_t duplicate_suppressed = running.load() ? duplicate_edge_suppressed_count.load() : retained_duplicate_edge_suppressed_count.load();
        const uint64_t outside_suppressed = running.load() ? outside_suppressed_count.load() : retained_outside_suppressed_count.load();
        const uint64_t hot_bypass_entries = running.load() ? hot_bypass_entry_count.load() : retained_hot_bypass_entry_count.load();
        const uint64_t capture_hits = running.load() ? trigger_capture_hit_count.load() : retained_trigger_capture_hit_count.load();
        const DWORD capture_last_thread = running.load() ? trigger_capture_last_thread_id.load() : retained_trigger_capture_last_thread_id.load();
        const uint32_t capture_waiting = trigger_capture_waiting_count.load();
        const uintptr_t region_address = ResolveStatusAddress(*this);
        out << L" hits=" << FlowHitPolicyName(options.hit_policy)
            << L" idle_escape=" << options.hot_bypass_threshold
            << L" scope=" << (options.trace_outside_modules ? L"all" : L"tracked")
            << L" observe=" << ObservationStateName(observation_state)
            << L" region=" << ExecutionRegionName(*this, region_address)
            << L" depth=" << depth
            << L" steps=" << steps
            << L" events=" << events;
        if (duplicate_suppressed != 0)
        {
            out << L" dup_suppressed=" << duplicate_suppressed;
        }
        if (outside_suppressed != 0)
        {
            out << L" outside_suppressed=" << outside_suppressed;
        }
        if (hot_bypass_entries != 0)
        {
            out << L" hot_bypass_count=" << hot_bypass_entries;
        }
        if (options.auto_select_thread && resolved_trigger_address != 0)
        {
            out << L" capture_waiting=" << capture_waiting;
            out << L" capture_hits=" << capture_hits;
            if (capture_last_thread != 0)
            {
                out << L" capture_last=" << capture_last_thread;
            }
        }
        if (suppressed_transition_streak != 0)
        {
            out << L" hot_streak=" << suppressed_transition_streak;
        }
        AppendObservedAddresses(out, *this);
        if (hot_bypass_resume != 0)
        {
            out << L" hot_resume=0x" << std::hex << hot_bypass_resume << std::dec;
        }
        if (hot_bypass_return != 0)
        {
            out << L" hot_return=0x" << std::hex << hot_bypass_return << std::dec;
        }
        return out.str();
    }

    std::vector<ModuleRange> Session::Impl::ModuleRanges() const
    {
        std::lock_guard<std::mutex> lock(state_lock);
        return module_ranges;
    }
}
