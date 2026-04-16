#include "pch.h"
#include "autostart/VDTraceAutoStartHelperInternal.h"

#include "tools/VDTraceControlSupport.h"

#include <fstream>

namespace vdtrace::autostart
{
    std::atomic<bool> g_bootstrap_started = false;
    std::atomic<bool> g_bootstrap_immediate_started = false;

    DWORD RunAutoStart(bool skip_wait)
    {
        const auto config_path = ReadAutoStartConfigPathFromEnvironment();
        RuntimeLog log(ReadAutoStartLogPathFromEnvironment());

        AutoStartConfig config = {};
        std::wstring error;
        const auto effective_config_path = config_path.empty() ? DefaultConfigPath() : std::filesystem::path(config_path);
        if (!LoadConfig(effective_config_path, config, error))
        {
            log.Append(L"读取自动启动配置失败: " + error);
            return 1;
        }

        log.Append(L"helper 已启动，配置文件=" + effective_config_path.wstring());
        log.Append(L"当前进程=" + helper_detail::CurrentProcessPath());
        log.Append(L"Agent 路径=" + config.trace.agent_path);

        if (!skip_wait && !WaitForConfiguredTiming(config, log, error))
        {
            log.Append(L"等待目标时机失败: " + error);
            return 2;
        }
        if (skip_wait)
        {
            log.Append(L"跳过等待时机，直接拉起 Agent。");
        }

        const HMODULE agent = LoadLibraryW(config.trace.agent_path.c_str());
        if (agent == nullptr)
        {
            log.Append(L"LoadLibraryW(Agent) 失败，error=" + std::to_wstring(GetLastError()));
            return 3;
        }

        using BootstrapFn = BOOL(WINAPI *)();
        const auto bootstrap = reinterpret_cast<BootstrapFn>(GetProcAddress(agent, "vdtrace_loader_bootstrap"));
        if (bootstrap == nullptr)
        {
            log.Append(L"Agent 缺少 vdtrace_loader_bootstrap 导出。");
            return 4;
        }

        if (!bootstrap())
        {
            log.Append(L"Agent bootstrap 返回失败。");
            return 5;
        }

        const DWORD pid = GetCurrentProcessId();
        if (!vdtrace::tools::WaitForPipeReady(pid, config.launch.agent_timeout_ms))
        {
            log.Append(L"等待 Agent IPC 就绪超时。");
            return 6;
        }

        const std::wstring trigger_point = config.trace.trigger_enabled ? config.trace.trigger_point : L"";
        const std::wstring depth_filter_spec = helper_detail::BuildDepthFilterSpec(config.trace);
        const auto configure_result = vdtrace::tools::Configure(
            pid,
            config.trace.thread_id,
            config.trace.modules,
            config.trace.output_path,
            config.trace.max_events,
            config.trace.trace_outside_modules,
            config.trace.backend,
            config.trace.control_flow_only,
            config.trace.max_call_depth,
            depth_filter_spec,
            config.trace.hit_policy,
            config.trace.hot_bypass_threshold,
            config.trace.enhanced_sampling,
            config.trace.auto_select_thread,
            config.trace.block_main_thread,
            false,
            trigger_point,
            config.trace.probe_spec,
            config.trace.stop_on_root_return,
            config.trace.async_thread_handoff,
            config.launch.agent_timeout_ms);
        if (!configure_result.success)
        {
            log.Append(L"自动 configure 失败: " + configure_result.message);
            return 7;
        }

        log.Append(L"自动 configure 成功。");
        const auto start_result = vdtrace::tools::Start(pid, config.launch.agent_timeout_ms);
        if (!start_result.success)
        {
            log.Append(L"自动 start 失败: " + start_result.message);
            return 8;
        }

        log.Append(L"自动 start 成功。");
        return 0;
    }

    DWORD WINAPI AutoStartThreadMain(void *)
    {
        return RunAutoStart(false);
    }

    DWORD WINAPI AutoStartImmediateThreadMain(void *)
    {
        return RunAutoStart(true);
    }

    RuntimeLog::RuntimeLog(std::filesystem::path path)
        : path_(std::move(path))
    {
        if (path_.empty())
        {
            path_ = helper_detail::BuildDefaultLogPath();
        }
        else
        {
            std::filesystem::path rewritten = path_;
            const std::wstring stem = rewritten.stem().wstring();
            const std::wstring extension = rewritten.extension().wstring();
            rewritten.replace_filename(stem + L"-" + std::to_wstring(GetCurrentProcessId()) + extension);
            path_ = std::move(rewritten);
        }

        std::error_code ec;
        std::filesystem::create_directories(path_.parent_path(), ec);
    }

    void RuntimeLog::Append(const std::wstring &text)
    {
        SYSTEMTIME now = {};
        GetLocalTime(&now);
        std::ofstream output(path_, std::ios::app | std::ios::binary);
        if (!output.is_open())
        {
            return;
        }

        std::lock_guard<std::mutex> lock(lock_);
        std::ostringstream line;
        line << "["
             << std::setw(2) << std::setfill('0') << now.wHour << ":"
             << std::setw(2) << std::setfill('0') << now.wMinute << ":"
             << std::setw(2) << std::setfill('0') << now.wSecond << "] "
             << helper_detail::NarrowUtf8(text) << "\n";
        const std::string rendered = line.str();
        output.write(rendered.data(), static_cast<std::streamsize>(rendered.size()));
    }

    void RuntimeLog::AppendAnsi(const std::string &text)
    {
        Append(std::wstring(text.begin(), text.end()));
    }

    const std::filesystem::path &RuntimeLog::Path() const
    {
        return path_;
    }

    std::filesystem::path ReadAutoStartConfigPathFromEnvironment()
    {
        const std::wstring value = helper_detail::ReadEnvironmentText(L"VDTRACE_AUTOSTART_CONFIG");
        return value.empty() ? std::filesystem::path() : std::filesystem::path(value);
    }

    std::filesystem::path ReadAutoStartLogPathFromEnvironment()
    {
        const std::wstring value = helper_detail::ReadEnvironmentText(L"VDTRACE_AUTOSTART_LOG");
        return value.empty() ? std::filesystem::path() : std::filesystem::path(value);
    }
}

extern "C"
{
    __declspec(dllexport) BOOL vdtrace_loader_bootstrap(void)
    {
        bool expected = false;
        if (!vdtrace::autostart::g_bootstrap_started.compare_exchange_strong(expected, true))
        {
            return TRUE;
        }

        const HANDLE thread = CreateThread(nullptr, 0, vdtrace::autostart::AutoStartThreadMain, nullptr, 0, nullptr);
        if (thread == nullptr)
        {
            vdtrace::autostart::g_bootstrap_started.store(false);
            return FALSE;
        }

        CloseHandle(thread);
        return TRUE;
    }

    __declspec(dllexport) BOOL vdtrace_loader_bootstrap_immediate(void)
    {
        bool expected = false;
        if (!vdtrace::autostart::g_bootstrap_immediate_started.compare_exchange_strong(expected, true))
        {
            return TRUE;
        }

        const HANDLE thread = CreateThread(nullptr, 0, vdtrace::autostart::AutoStartImmediateThreadMain, nullptr, 0, nullptr);
        if (thread == nullptr)
        {
            vdtrace::autostart::g_bootstrap_immediate_started.store(false);
            return FALSE;
        }

        CloseHandle(thread);
        return TRUE;
    }
}
