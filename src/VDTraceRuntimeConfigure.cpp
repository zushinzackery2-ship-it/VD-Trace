#include "pch.h"
#include "VDTraceInternal.h"

namespace vdtrace
{
    namespace
    {
        FlowHitPolicy NormalizeHitPolicy(FlowHitPolicy policy)
        {
            switch (policy)
            {
            case FlowHitPolicy::EveryHit:
                return FlowHitPolicy::EveryHit;
            case FlowHitPolicy::FirstSeen:
            default:
                return FlowHitPolicy::FirstSeen;
            }
        }

        BackendMode ResolveBackendMode(const Options &options)
        {
            switch (options.backend)
            {
            case TraceBackend::TfFullTrace:
                return BackendMode::TrapFlagContext;
            case TraceBackend::DrControlFlow:
            default:
                return options.control_flow_only
                    ? BackendMode::HardwareFlow
                    : BackendMode::TrapFlagContext;
            }
        }

        bool ValidateBackendOptions(Options &options, std::wstring &error)
        {
            if (options.backend == TraceBackend::DrControlFlow
                || options.backend == TraceBackend::TfFullTrace)
            {
                return true;
            }

            error = L"backend 只支持 DR / TF。";
            return false;
        }
    }

    bool Session::Impl::Configure(const Options &new_options, std::wstring &error)
    {
        std::lock_guard<std::mutex> lock(state_lock);
        if (running.load())
        {
            error = L"trace 正在运行，不能重新配置。";
            return false;
        }

        if (new_options.module_names.empty() && !new_options.trace_outside_modules)
        {
            error = L"未指定模块时必须启用 trace_outside_modules。";
            return false;
        }

        StopTriggerCaptureRefreshWorker(*this);
        ReleaseTriggerCaptureThreads(*this, true);

        std::vector<ModuleRange> resolved_ranges;
        for (const auto &name : new_options.module_names)
        {
            ModuleRange range = {};
            if (!ResolveModuleRange(name, range, error))
            {
                return false;
            }
            resolved_ranges.push_back(std::move(range));
        }

        options = new_options;
        options.hit_policy = NormalizeHitPolicy(options.hit_policy);
        if (!ValidateBackendOptions(options, error))
        {
            return false;
        }
        if (!ResolveTriggerAddress(options, resolved_trigger_address, error))
        {
            return false;
        }
        configured_trigger_address = resolved_trigger_address;
        if (!ParseProbeSpec(options.probe_spec, value_probes, error))
        {
            return false;
        }

        if (options.queue_trigger_threads
            && (!options.auto_select_thread || resolved_trigger_address == 0))
        {
            error = L"线程轮转模式需要同时启用定点触发和线程自动捕获。";
            return false;
        }

        if (options.block_main_thread
            && (!options.auto_select_thread || resolved_trigger_address == 0))
        {
            error = L"屏蔽主线程需要同时启用定点触发和线程自动捕获。";
            return false;
        }

        if (options.queue_trigger_threads && !options.stop_on_root_return)
        {
            error = L"线程轮转模式需要启用单次调用即停。";
            return false;
        }

        DWORD discovered_main_thread_id = 0;
        if (!GuessCurrentProcessMainThread(discovered_main_thread_id, error))
        {
            return false;
        }
        main_thread_id.store(discovered_main_thread_id);

        if (options.auto_select_thread && resolved_trigger_address != 0)
        {
            options.thread_id = 0;
        }
        else if (options.thread_id == 0)
        {
            options.thread_id = discovered_main_thread_id;
        }

        std::sort(
            resolved_ranges.begin(),
            resolved_ranges.end(),
            [](const ModuleRange &left, const ModuleRange &right)
            {
                return left.base < right.base;
            });
        module_ranges = std::move(resolved_ranges);
        EnumerateSystemModuleRanges(system_module_ranges);
        if (!ParseDepthFilterSpec(options.depth_filter_spec, module_ranges, system_module_ranges, depth_filters, error))
        {
            return false;
        }
        async_probes = KnownAsyncProbes();
        backend_mode = ResolveBackendMode(options);
        lightweight_module_capture = options.callback == TextFileRecorder::Callback;
        seen_edges.reserve(DetermineSeenEdgeReserve(options));
        active_thread_id.store(options.thread_id);
        step_count.store(0);
        event_count.store(0);
        seen_edges.clear();
        call_depth_offset = 0;
        ResetThreadObservationState(*this);
        RememberCurrentProcessThreads(*this);
        ClearExecutionSummary(*this);
        configured.store(true);
        return true;
    }
}
