#include "pch.h"
#include "lite/LiteTraceRuntime.h"

#include <fstream>

namespace vdtrace::lite
{
    namespace
    {
        std::filesystem::path BuildLogPath()
        {
            const std::filesystem::path base = LiteTraceModuleDirectory();
            std::error_code ec;
            std::filesystem::create_directories(base / L"traces", ec);
            return base / L"traces" / (L"LiteTrace-" + std::to_wstring(GetCurrentProcessId()) + L".log");
        }

        std::string NarrowUtf8(const std::wstring &text)
        {
            if (text.empty())
            {
                return {};
            }

            const int count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (count <= 1)
            {
                return {};
            }

            std::string result(static_cast<size_t>(count), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), count, nullptr, nullptr);
            if (!result.empty() && result.back() == '\0')
            {
                result.pop_back();
            }
            return result;
        }

        void LogLine(const std::filesystem::path &path, const std::wstring &text)
        {
            std::ofstream output(path, std::ios::app | std::ios::binary);
            if (!output.is_open())
            {
                return;
            }

            SYSTEMTIME now = {};
            GetLocalTime(&now);
            std::ostringstream line;
            line << "["
                 << std::setw(2) << std::setfill('0') << now.wHour << ":"
                 << std::setw(2) << std::setfill('0') << now.wMinute << ":"
                 << std::setw(2) << std::setfill('0') << now.wSecond << "] "
                 << NarrowUtf8(text) << "\n";
            const std::string rendered = line.str();
            output.write(rendered.data(), static_cast<std::streamsize>(rendered.size()));
        }

        bool BuildTraceOptions(const LiteTraceConfig &config, Options &options, std::wstring &output_path, std::wstring &error)
        {
            options = {};

            // LiteTrace is a pure trigger-point tracer: the hardware breakpoint fires on
            // whatever thread reaches trigger_point, and that thread is traced. There is
            // deliberately no thread pinning, thread rotation, main-thread blocking, or
            // cross-thread handoff - those belong to VDTraceAutoStart / the Agent, not to
            // the lightweight in-process path. These stay hardcoded to the "current hit
            // thread" semantics.
            options.thread_id = 0;
            options.auto_select_thread = true;
            options.block_main_thread = false;
            options.queue_trigger_threads = false;
            options.async_thread_handoff = false;

            options.module_names = SplitModuleNames(config.modules);
            output_path = ResolveLiteOutputPath(config);
            options.output_path = output_path;
            options.max_events = config.max_events;
            options.trace_outside_modules = config.trace_outside_modules;
            options.backend = config.backend;
            options.control_flow_only = config.control_flow_only;
            options.max_call_depth = config.max_call_depth;
            options.depth_filter_spec = BuildLiteDepthFilterSpec(config);
            options.hit_policy = config.hit_policy;
            options.hot_bypass_threshold = config.hot_bypass_threshold;
            options.sim_fast_forward = config.sim_fast_forward;
            options.sim_fast_forward_indirect = config.sim_fast_forward_indirect;
            options.enhanced_sampling = config.enhanced_sampling;
            options.probe_spec = config.probe_spec;
            options.stop_on_root_return = config.stop_on_root_return;

            if (config.trigger_enabled && !config.trigger_point.empty())
            {
                return ParseLiteTriggerPoint(config.trigger_point, options.trigger_module_name, options.trigger_address, error);
            }

            return true;
        }

        void WaitForTraceFinish(const Session &session)
        {
            while (session.IsRunning())
            {
                Sleep(50);
            }
        }
    }

    DWORD RunLiteTrace()
    {
        const std::filesystem::path log_path = BuildLogPath();
        const std::filesystem::path config_path = DefaultLiteTraceConfigPath();

        LiteTraceConfig config = {};
        std::wstring error;
        if (!LoadLiteTraceConfig(config_path, config, error))
        {
            LogLine(log_path, L"读取 LiteTrace.ini 失败: " + error);
            return 1;
        }

        LogLine(log_path, L"LiteTrace 已启动，配置=" + config_path.wstring());

        std::wstring output_path;
        Options options = {};
        if (!BuildTraceOptions(config, options, output_path, error))
        {
            LogLine(log_path, L"解析触发点失败: " + error);
            return 2;
        }

        auto recorder = std::make_unique<TextFileRecorder>(output_path, options);
        if (!recorder->IsOpen())
        {
            LogLine(log_path, L"打开输出文件失败: " + output_path);
            return 3;
        }

        options.callback = TextFileRecorder::Callback;
        options.callback_context = recorder.get();

        Session session;
        if (!session.Configure(options, error))
        {
            LogLine(log_path, L"Configure 失败: " + error);
            return 4;
        }

        if (!session.Start(error))
        {
            LogLine(log_path, L"Start 失败: " + error);
            return 5;
        }

        const std::wstring trigger_text = options.trigger_module_name.empty() && options.trigger_address == 0
            ? L"(即时)"
            : config.trigger_point;
        LogLine(log_path, L"trace 已启动，output=" + output_path + L" trigger=" + trigger_text);

        WaitForTraceFinish(session);
        session.Stop(error);

        const std::wstring summary = session.DescribeState();
        const uint64_t events = session.EventCount();
        recorder.reset();

        LogLine(log_path, L"trace 结束，events=" + std::to_wstring(events) + L" state=" + summary);

        if (config.exit_process_on_finish)
        {
            LogLine(log_path, L"exit_process_on_finish=true，结束进程。");
            ExitProcess(0);
        }

        return 0;
    }

    DWORD WINAPI LiteTraceThreadMain(void *)
    {
        return RunLiteTrace();
    }
}
