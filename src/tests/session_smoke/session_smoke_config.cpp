#include "session_smoke_cli.h"

#include <sstream>

namespace session_smoke
{
    namespace
    {
        TraceRunOptions MakeOptions(
            void (*routine)(),
            bool trace_outside_modules,
            bool control_flow_only,
            uint32_t max_call_depth,
            vdtrace::FlowHitPolicy hit_policy,
            uint64_t max_events,
            bool stop_on_root_return,
            bool auto_select_thread,
            bool queue_trigger_threads,
            bool async_thread_handoff,
            bool block_main_thread)
        {
            TraceRunOptions options = {};
            options.routine = routine;
            options.trace_outside_modules = trace_outside_modules;
            options.control_flow_only = control_flow_only;
            options.max_call_depth = max_call_depth;
            options.hit_policy = hit_policy;
            options.max_events = max_events;
            options.stop_on_root_return = stop_on_root_return;
            options.auto_select_thread = auto_select_thread;
            options.queue_trigger_threads = queue_trigger_threads;
            options.async_thread_handoff = async_thread_handoff;
            options.block_main_thread = block_main_thread;
            return options;
        }

        std::wstring HexText(uintptr_t value)
        {
            std::wostringstream out;
            out << L"0x" << std::hex << value;
            return out.str();
        }

        std::wstring BuildProbeSpec()
        {
            return HexText(ProbeObservedMidpoint())
                + L"->mem:" + HexText(ProbeBytesAddress()) + L":16:keyiv"
                + L"|reg:rsp:rsp"
                + L"|ptr:rsp:16:stack";
        }

        std::wstring DepthText(uint32_t value)
        {
            if (value == vdtrace::kUnlimitedCallDepth)
            {
                return L"all";
            }

            if (value == 0)
            {
                return L"single";
            }

            return std::to_wstring(value);
        }

        std::wstring OutsideDepthFilter(uint32_t value)
        {
            return L"outside=" + DepthText(value);
        }

        std::wstring OutsideModeDepthFilter(uint32_t value, const wchar_t *mode)
        {
            return L"outside=" + DepthText(value) + L":" + mode;
        }

        std::wstring AnonymousDepthFilter(uint32_t value)
        {
            return L"anon=" + DepthText(value);
        }

        std::wstring AnonymousModeDepthFilter(uint32_t value, const wchar_t *mode)
        {
            return L"anon=" + DepthText(value) + L":" + mode;
        }

        std::wstring ModuleDepthFilter(const wchar_t *module_name, uint32_t value)
        {
            return std::wstring(L"module=") + module_name + L":" + DepthText(value);
        }

        std::wstring ModuleModeDepthFilter(const wchar_t *module_name, uint32_t value, const wchar_t *mode)
        {
            return std::wstring(L"module=") + module_name + L":" + DepthText(value) + L":" + mode;
        }
    }

    TraceRunOptions MakeSessionSmokeOptions(
        void (*routine)(),
        bool trace_outside_modules,
        bool control_flow_only,
        uint32_t max_call_depth,
        vdtrace::FlowHitPolicy hit_policy,
        uint64_t max_events,
        bool stop_on_root_return,
        bool auto_select_thread,
        bool queue_trigger_threads,
        bool async_thread_handoff,
        bool block_main_thread)
    {
        return MakeOptions(
            routine,
            trace_outside_modules,
            control_flow_only,
            max_call_depth,
            hit_policy,
            max_events,
            stop_on_root_return,
            auto_select_thread,
            queue_trigger_threads,
            async_thread_handoff,
            block_main_thread);
    }

    SessionSmokeConfig BuildSessionSmokeConfig()
    {
        SessionSmokeConfig config = {};
        config.outside_depth_options = MakeSessionSmokeOptions(&CrossModuleHelperEntry, true, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0);
        config.outside_depth_options.depth_filter_spec = OutsideDepthFilter(2);
        config.outside_tf_options = MakeSessionSmokeOptions(&CrossModuleHelperEntry, true, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0);
        config.outside_tf_options.depth_filter_spec = OutsideModeDepthFilter(2, L"tf");
        config.module_depth_options = MakeSessionSmokeOptions(&CrossModuleHelperEntry, true, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0);
        config.module_depth_options.depth_filter_spec = ModuleDepthFilter(L"VDTraceTriggerWaitHelper.dll", 2);
        config.module_tf_options = MakeSessionSmokeOptions(&CrossModuleHelperEntry, true, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0);
        config.module_tf_options.depth_filter_spec = ModuleModeDepthFilter(L"VDTraceTriggerWaitHelper.dll", 2, L"tf");
        config.anonymous_depth_options = MakeSessionSmokeOptions(&AnonymousExecEntry, false, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0);
        config.anonymous_depth_options.depth_filter_spec = AnonymousDepthFilter(2);
        config.anonymous_tf_options = MakeSessionSmokeOptions(&AnonymousExecEntry, false, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0);
        config.anonymous_tf_options.depth_filter_spec = AnonymousModeDepthFilter(2, L"tf");
        config.probe_options = MakeSessionSmokeOptions(&ProbeObservedEntry, false, true, 1, vdtrace::FlowHitPolicy::EveryHit, 0, true);
        config.probe_options.probe_spec = BuildProbeSpec();
        config.delayed_all_events_options = MakeSessionSmokeOptions(&SameLevelEntry, false, false, 1, vdtrace::FlowHitPolicy::EveryHit, 32, false, true);
        config.unity_follow_options = MakeSessionSmokeOptions(&UnityWorkerAssetEntry, true, true, 4, vdtrace::FlowHitPolicy::FirstSeen, 96, false, true);
        config.probe_queue_options = MakeSessionSmokeOptions(&ProbeObservedEntry, false, true, 1, vdtrace::FlowHitPolicy::EveryHit, 0, true, true, true);
        config.probe_queue_options.probe_spec = BuildProbeSpec();
        return config;
    }
}
