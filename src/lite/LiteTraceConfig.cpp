#include "pch.h"
#include "lite/LiteTraceConfig.h"

#include "autostart/VDTraceAutoStartConfigInternal.h"

namespace vdtrace::lite
{
    namespace as = vdtrace::autostart::detail;

    namespace
    {
        bool ParseExecutionMode(const std::wstring &text, TraceExecutionMode &mode)
        {
            const std::wstring trimmed = as::TrimAutoStartText(text);
            if (trimmed.empty() || _wcsicmp(trimmed.c_str(), L"edge") == 0)
            {
                mode = TraceExecutionMode::Edge;
                return true;
            }

            if (_wcsicmp(trimmed.c_str(), L"tf") == 0)
            {
                mode = TraceExecutionMode::TrapFlag;
                return true;
            }

            return false;
        }

        bool ResolveBackend(const as::SectionMap &sections, LiteTraceConfig &config, std::wstring &error)
        {
            const std::wstring backend_text = as::TrimAutoStartText(as::GetAutoStartValue(sections, L"trace", L"backend", L""));
            if (_wcsicmp(backend_text.c_str(), L"tf") == 0)
            {
                config.backend = TraceBackend::TfFullTrace;
                config.control_flow_only = false;
                return true;
            }

            if (!backend_text.empty() && _wcsicmp(backend_text.c_str(), L"dr") != 0)
            {
                error = L"trace.backend 只支持 DR / TF。";
                return false;
            }

            config.backend = as::ParseAutoStartBool(as::GetAutoStartValue(sections, L"trace", L"all_events", L"false"), false)
                ? TraceBackend::TfFullTrace
                : TraceBackend::DrControlFlow;
            config.control_flow_only = config.backend != TraceBackend::TfFullTrace;
            return true;
        }

        bool ResolveDepthFilters(const as::SectionMap &sections, LiteTraceConfig &config, std::wstring &error)
        {
            if (!as::ParseAutoStartCallDepth(as::GetAutoStartValue(sections, L"trace", L"call_depth", L"4"), config.max_call_depth))
            {
                error = L"trace.call_depth 无效。使用 single / all / 数字。";
                return false;
            }

            const std::wstring outside_depth = as::TrimAutoStartText(as::GetAutoStartValue(sections, L"trace", L"outside_call_depth", L""));
            if (!ParseExecutionMode(as::GetAutoStartValue(sections, L"trace", L"outside_execution_mode", L"edge"), config.outside_module_execution_mode))
            {
                error = L"trace.outside_execution_mode 无效。使用 EDGE / TF。";
                return false;
            }
            if (!outside_depth.empty())
            {
                if (!as::ParseAutoStartCallDepth(outside_depth, config.outside_module_depth))
                {
                    error = L"trace.outside_call_depth 无效。使用 single / all / 数字。";
                    return false;
                }
                config.has_outside_module_depth = true;
            }

            const std::wstring anon_depth = as::TrimAutoStartText(as::GetAutoStartValue(sections, L"trace", L"anonymous_exec_call_depth", L""));
            if (!ParseExecutionMode(as::GetAutoStartValue(sections, L"trace", L"anonymous_exec_execution_mode", L"edge"), config.anonymous_exec_execution_mode))
            {
                error = L"trace.anonymous_exec_execution_mode 无效。使用 EDGE / TF。";
                return false;
            }
            if (!anon_depth.empty())
            {
                if (!as::ParseAutoStartCallDepth(anon_depth, config.anonymous_exec_depth))
                {
                    error = L"trace.anonymous_exec_call_depth 无效。使用 single / all / 数字。";
                    return false;
                }
                config.has_anonymous_exec_depth = true;
            }

            config.module_call_depths = as::TrimAutoStartText(as::GetAutoStartValue(sections, L"trace", L"module_call_depths", L""));
            return true;
        }

        void ResolveTraceScalars(const as::SectionMap &sections, LiteTraceConfig &config)
        {
            uint64_t numeric = 0;
            if (as::ParseAutoStartUint64(as::GetAutoStartValue(sections, L"trace", L"max_events", L"0"), numeric))
            {
                config.max_events = numeric;
            }
            if (as::ParseAutoStartUint64(as::GetAutoStartValue(sections, L"trace", L"idle_escape_threshold", L"8"), numeric))
            {
                config.hot_bypass_threshold = static_cast<uint32_t>(std::min<uint64_t>(numeric, 0xFFFFFFFFull));
            }
        }

        bool ResolveLiteSection(const as::SectionMap &sections, LiteTraceConfig &config, std::wstring &error)
        {
            const std::wstring mode_text = as::TrimAutoStartText(as::GetAutoStartValue(sections, L"lite", L"mode", L"step"));
            if (mode_text.empty() || _wcsicmp(mode_text.c_str(), L"step") == 0)
            {
                config.mode = LiteMode::Step;
            }
            else if (_wcsicmp(mode_text.c_str(), L"specified") == 0)
            {
                config.mode = LiteMode::Specified;
            }
            else
            {
                error = L"lite.mode 只支持 step / specified。";
                return false;
            }

            config.exit_process_on_finish = as::ParseAutoStartBool(as::GetAutoStartValue(sections, L"lite", L"exit_process_on_finish", L"false"), false);
            return true;
        }

        // Force the config to only carry the knobs the selected mode actually uses, so
        // logging and the mapped Options reflect the effective behaviour.
        void ApplyLiteMode(LiteTraceConfig &config)
        {
            if (config.mode == LiteMode::Specified)
            {
                // Specified mode ends at end_point (or root return); step count is off.
                config.max_events = 0;
            }
            else
            {
                // Step mode ends after max_events; ignore every end-point knob.
                config.end_point.clear();
                config.stop_on_root_return = false;
            }
        }
    }

    bool LoadLiteTraceConfig(const std::filesystem::path &path, LiteTraceConfig &config, std::wstring &error)
    {
        error.clear();
        config = {};
        config.config_path = path;
        config.config_directory = path.parent_path().empty() ? std::filesystem::current_path() : path.parent_path();

        if (!std::filesystem::exists(path))
        {
            if (!as::WriteAutoStartUtf8TextFile(path, BuildDefaultLiteTraceConfigText()))
            {
                error = L"LiteTrace.ini 不存在，且默认模板写入失败。";
                return false;
            }
        }

        std::wstring text;
        if (!as::ReadAutoStartUtf8TextFile(path, text))
        {
            error = L"读取 LiteTrace.ini 失败。";
            return false;
        }

        as::SectionMap sections;
        as::ParseAutoStartIniText(text, sections);

        config.modules = as::GetAutoStartValue(sections, L"trace", L"modules", L"");
        config.output_path = as::GetAutoStartValue(sections, L"trace", L"output_path", L".\\traces\\LiteTrace.log");
        config.trigger_point = as::GetAutoStartValue(sections, L"trace", L"trigger_point", L"");
        config.end_point = as::GetAutoStartValue(sections, L"trace", L"end_point", L"");
        config.probe_spec = as::GetAutoStartValue(sections, L"trace", L"probe_spec", L"");
        config.trace_outside_modules = as::ParseAutoStartBool(as::GetAutoStartValue(sections, L"trace", L"trace_outside_modules", L"false"), false);
        config.enhanced_sampling = as::ParseAutoStartBool(as::GetAutoStartValue(sections, L"trace", L"enhanced_sampling", L"false"), false);
        config.sim_fast_forward = as::ParseAutoStartBool(as::GetAutoStartValue(sections, L"trace", L"sim_fast_forward", L"false"), false);
        config.sim_fast_forward_indirect = as::ParseAutoStartBool(as::GetAutoStartValue(sections, L"trace", L"sim_fast_forward_indirect", L"false"), false);
        config.trigger_enabled = as::ParseAutoStartBool(as::GetAutoStartValue(sections, L"trace", L"trigger_enabled", L"true"), true);
        config.stop_on_root_return = as::ParseAutoStartBool(as::GetAutoStartValue(sections, L"trace", L"root_stop_on_return", L"false"), false);
        config.hit_policy = as::ParseAutoStartBool(as::GetAutoStartValue(sections, L"trace", L"repeat_hits", L"false"), false)
            ? FlowHitPolicy::EveryHit
            : FlowHitPolicy::FirstSeen;

        if (!ResolveBackend(sections, config, error))
        {
            return false;
        }

        ResolveTraceScalars(sections, config);

        if (!ResolveDepthFilters(sections, config, error))
        {
            return false;
        }

        if (!ResolveLiteSection(sections, config, error))
        {
            return false;
        }

        if (!config.trigger_enabled)
        {
            config.trigger_point.clear();
        }

        ApplyLiteMode(config);

        return true;
    }
}
