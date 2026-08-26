#include "pch.h"
#include "autostart/VDTraceAutoStartConfigInternal.h"

namespace vdtrace::autostart
{
    namespace
    {
        bool ParseTraceExecutionMode(const std::wstring &text, TraceExecutionMode &mode)
        {
            const std::wstring trimmed = detail::TrimAutoStartText(text);
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

        std::filesystem::path FindExistingDefaultConfigPath()
        {
            const std::filesystem::path working_directory_path = std::filesystem::current_path() / L"vdtrace_autostart.ini";
            if (std::filesystem::exists(working_directory_path))
            {
                return working_directory_path.lexically_normal();
            }

            const std::filesystem::path module_directory = detail::GetAutoStartModuleDirectory();
            const std::filesystem::path repo_root_path = module_directory.parent_path().parent_path() / L"vdtrace_autostart.ini";
            if (std::filesystem::exists(repo_root_path))
            {
                return repo_root_path.lexically_normal();
            }

            const std::filesystem::path module_directory_path = module_directory / L"vdtrace_autostart.ini";
            if (std::filesystem::exists(module_directory_path))
            {
                return module_directory_path.lexically_normal();
            }

            return working_directory_path.lexically_normal();
        }
    }

    std::filesystem::path DefaultConfigPath()
    {
        return FindExistingDefaultConfigPath();
    }

    bool LoadConfig(const std::filesystem::path &path, AutoStartConfig &config, std::wstring &error)
    {
        error.clear();
        config = {};
        config.config_path = path;
        config.config_directory = path.parent_path().empty() ? std::filesystem::current_path() : path.parent_path();

        if (!std::filesystem::exists(path))
        {
            if (!detail::WriteAutoStartUtf8TextFile(path, detail::BuildDefaultAutoStartConfigText()))
            {
                error = L"自动启动 ini 不存在，且默认配置写入失败。";
                return false;
            }
        }

        std::wstring text;
        if (!detail::ReadAutoStartUtf8TextFile(path, text))
        {
            error = L"读取自动启动 ini 失败。";
            return false;
        }

        detail::SectionMap sections;
        detail::ParseAutoStartIniText(text, sections);

        config.launch.game_path = detail::ResolveConfigRelativePath(config.config_directory, detail::GetAutoStartValue(sections, L"launch", L"game_path", L"")).wstring();
        config.launch.working_directory = detail::ResolveConfigRelativePath(config.config_directory, detail::GetAutoStartValue(sections, L"launch", L"working_directory", L"")).wstring();
        config.launch.arguments = detail::GetAutoStartValue(sections, L"launch", L"arguments", L"");
        config.launch.helper_path = detail::ResolveConfigRelativePath(config.config_directory, detail::GetAutoStartValue(sections, L"launch", L"helper_path", L".\\bin\\release\\VDTraceAutoStart.dll")).wstring();
        config.launch.wait_for_trace_end = detail::ParseAutoStartBool(detail::GetAutoStartValue(sections, L"launch", L"wait_for_trace_end", L"true"), true);

        uint64_t numeric_value = 0;
        if (detail::ParseAutoStartUint64(detail::GetAutoStartValue(sections, L"launch", L"loader_timeout_ms", L"60000"), numeric_value))
        {
            config.launch.loader_timeout_ms = static_cast<DWORD>(std::min<uint64_t>(numeric_value, 0xFFFFFFFFull));
        }
        if (detail::ParseAutoStartUint64(detail::GetAutoStartValue(sections, L"launch", L"agent_timeout_ms", L"30000"), numeric_value))
        {
            config.launch.agent_timeout_ms = static_cast<DWORD>(std::min<uint64_t>(numeric_value, 0xFFFFFFFFull));
        }
        if (detail::ParseAutoStartUint64(detail::GetAutoStartValue(sections, L"launch", L"trace_start_timeout_ms", L"120000"), numeric_value))
        {
            config.launch.trace_start_timeout_ms = static_cast<DWORD>(std::min<uint64_t>(numeric_value, 0xFFFFFFFFull));
        }
        if (detail::ParseAutoStartUint64(detail::GetAutoStartValue(sections, L"launch", L"trace_finish_timeout_ms", L"1800000"), numeric_value))
        {
            config.launch.trace_finish_timeout_ms = static_cast<DWORD>(std::min<uint64_t>(numeric_value, 0xFFFFFFFFull));
        }

        const std::wstring wait_mode_text = detail::GetAutoStartValue(sections, L"wait", L"mode", L"bepinex_il2cpp_scene_change");
        if (_wcsicmp(wait_mode_text.c_str(), L"disabled") == 0)
        {
            config.wait.mode = WaitMode::Disabled;
        }
        else if (_wcsicmp(wait_mode_text.c_str(), L"bepinex_il2cpp_scene_change") == 0)
        {
            config.wait.mode = WaitMode::BepInExIl2CppSceneChange;
        }
        else
        {
            error = L"wait.mode 无效。当前只支持 disabled / bepinex_il2cpp_scene_change。";
            return false;
        }

        config.wait.module_name = detail::GetAutoStartValue(sections, L"wait", L"module_name", L"GameAssembly.dll");
        config.wait.invoke_export = detail::NarrowAutoStartUtf8(detail::GetAutoStartValue(sections, L"wait", L"invoke_export", L"il2cpp_runtime_invoke"));
        config.wait.method_name_export = detail::NarrowAutoStartUtf8(detail::GetAutoStartValue(sections, L"wait", L"method_name_export", L"il2cpp_method_get_name"));
        config.wait.target_method_name = detail::NarrowAutoStartUtf8(detail::GetAutoStartValue(sections, L"wait", L"target_method_name", L"Internal_ActiveSceneChanged"));
        if (detail::ParseAutoStartUint64(detail::GetAutoStartValue(sections, L"wait", L"module_poll_interval_ms", L"50"), numeric_value))
        {
            config.wait.module_poll_interval_ms = static_cast<DWORD>(std::min<uint64_t>(numeric_value, 0xFFFFFFFFull));
        }
        if (detail::ParseAutoStartUint64(detail::GetAutoStartValue(sections, L"wait", L"wait_timeout_ms", L"600000"), numeric_value))
        {
            config.wait.wait_timeout_ms = static_cast<DWORD>(std::min<uint64_t>(numeric_value, 0xFFFFFFFFull));
        }

        config.trace.agent_path = detail::ResolveConfigRelativePath(config.config_directory, detail::GetAutoStartValue(sections, L"trace", L"agent_path", L".\\bin\\release\\VDTraceAgent.dll")).wstring();
        config.trace.modules = detail::GetAutoStartValue(sections, L"trace", L"modules", L"UnityPlayer.dll");
        config.trace.output_path = detail::GetAutoStartValue(sections, L"trace", L"output_path", L".\\traces\\VDTrace.log");
        config.trace.trigger_point = detail::GetAutoStartValue(sections, L"trace", L"trigger_point", L"");
        config.trace.probe_spec = detail::GetAutoStartValue(sections, L"trace", L"probe_spec", L"");
        config.trace.auto_select_thread = detail::ParseAutoStartBool(detail::GetAutoStartValue(sections, L"trace", L"auto_select_thread", L"true"), true);
        config.trace.block_main_thread = detail::ParseAutoStartBool(detail::GetAutoStartValue(sections, L"trace", L"block_main_thread", L"false"), false);
        config.trace.trace_outside_modules = detail::ParseAutoStartBool(detail::GetAutoStartValue(sections, L"trace", L"trace_outside_modules", L"false"), false);
        const std::wstring backend_text = detail::TrimAutoStartText(detail::GetAutoStartValue(sections, L"trace", L"backend", L""));
        if (_wcsicmp(backend_text.c_str(), L"tf") == 0)
        {
            config.trace.backend = TraceBackend::TfFullTrace;
            config.trace.control_flow_only = false;
        }
        else
        {
            if (!backend_text.empty() && _wcsicmp(backend_text.c_str(), L"dr") != 0)
            {
                error = L"trace.backend 只支持 DR / TF。";
                return false;
            }
            config.trace.backend = detail::ParseAutoStartBool(detail::GetAutoStartValue(sections, L"trace", L"all_events", L"false"), false)
                ? TraceBackend::TfFullTrace
                : TraceBackend::DrControlFlow;
            config.trace.control_flow_only = config.trace.backend != TraceBackend::TfFullTrace;
        }
        config.trace.enhanced_sampling = detail::ParseAutoStartBool(detail::GetAutoStartValue(sections, L"trace", L"enhanced_sampling", L"false"), false);
        config.trace.trigger_enabled = detail::ParseAutoStartBool(detail::GetAutoStartValue(sections, L"trace", L"trigger_enabled", L"true"), true);
        config.trace.stop_on_root_return = detail::ParseAutoStartBool(detail::GetAutoStartValue(sections, L"trace", L"root_stop_on_return", L"false"), false);
        config.trace.async_thread_handoff = detail::ParseAutoStartBool(detail::GetAutoStartValue(sections, L"trace", L"async_thread_handoff", L"true"), true);
        config.trace.hit_policy = detail::ParseAutoStartBool(detail::GetAutoStartValue(sections, L"trace", L"repeat_hits", L"false"), false)
            ? FlowHitPolicy::EveryHit
            : FlowHitPolicy::FirstSeen;

        if (detail::ParseAutoStartUint64(detail::GetAutoStartValue(sections, L"trace", L"thread_id", L"0"), numeric_value))
        {
            config.trace.thread_id = static_cast<DWORD>(std::min<uint64_t>(numeric_value, 0xFFFFFFFFull));
        }
        if (detail::ParseAutoStartUint64(detail::GetAutoStartValue(sections, L"trace", L"max_events", L"0"), numeric_value))
        {
            config.trace.max_events = numeric_value;
        }
        if (detail::ParseAutoStartUint64(detail::GetAutoStartValue(sections, L"trace", L"idle_escape_threshold", L"0"), numeric_value))
        {
            config.trace.hot_bypass_threshold = static_cast<uint32_t>(std::min<uint64_t>(numeric_value, 0xFFFFFFFFull));
        }
        if (!detail::ParseAutoStartCallDepth(detail::GetAutoStartValue(sections, L"trace", L"call_depth", L"4"), config.trace.max_call_depth))
        {
            error = L"trace.call_depth 无效。使用 single / all / 数字。";
            return false;
        }

        const std::wstring outside_call_depth = detail::TrimAutoStartText(detail::GetAutoStartValue(sections, L"trace", L"outside_call_depth", L""));
        const std::wstring outside_execution_mode = detail::TrimAutoStartText(detail::GetAutoStartValue(sections, L"trace", L"outside_execution_mode", L"edge"));
        if (!ParseTraceExecutionMode(outside_execution_mode, config.trace.outside_module_execution_mode))
        {
            error = L"trace.outside_execution_mode 无效。使用 EDGE / TF。";
            return false;
        }
        if (!outside_call_depth.empty())
        {
            if (!detail::ParseAutoStartCallDepth(outside_call_depth, config.trace.outside_module_depth))
            {
                error = L"trace.outside_call_depth 无效。使用 single / all / 数字。";
                return false;
            }

            config.trace.has_outside_module_depth = true;
        }

        const std::wstring anonymous_exec_call_depth = detail::TrimAutoStartText(detail::GetAutoStartValue(sections, L"trace", L"anonymous_exec_call_depth", L""));
        const std::wstring anonymous_exec_execution_mode = detail::TrimAutoStartText(detail::GetAutoStartValue(sections, L"trace", L"anonymous_exec_execution_mode", L"edge"));
        if (!ParseTraceExecutionMode(anonymous_exec_execution_mode, config.trace.anonymous_exec_execution_mode))
        {
            error = L"trace.anonymous_exec_execution_mode 无效。使用 EDGE / TF。";
            return false;
        }
        if (!anonymous_exec_call_depth.empty())
        {
            if (!detail::ParseAutoStartCallDepth(anonymous_exec_call_depth, config.trace.anonymous_exec_depth))
            {
                error = L"trace.anonymous_exec_call_depth 无效。使用 single / all / 数字。";
                return false;
            }

            config.trace.has_anonymous_exec_depth = true;
        }

        config.trace.module_call_depths = detail::TrimAutoStartText(detail::GetAutoStartValue(sections, L"trace", L"module_call_depths", L""));

        if (!config.trace.trigger_enabled)
        {
            config.trace.trigger_point.clear();
        }

        return true;
    }
}
