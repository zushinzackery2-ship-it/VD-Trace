#include "pch.h"
#include "tools/vdtrace_autostart_internal.h"
#include "tools/VDTraceControlSupport.h"

#include <unordered_set>

int wmain(int argc, wchar_t **argv)
{
    using namespace vdtrace::tools::autostart_cli;

    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    if (argc > 3)
    {
        PrintUsage();
        return 1;
    }

    const bool restore_only = argc >= 2 && std::wstring(argv[1]) == L"--restore";
    if (restore_only && argc > 3)
    {
        PrintUsage();
        return 1;
    }

    const std::filesystem::path config_path = (!restore_only && argc == 2)
        ? std::filesystem::path(argv[1])
        : (restore_only && argc == 3)
            ? std::filesystem::path(argv[2])
            : vdtrace::autostart::DefaultConfigPath();

    vdtrace::autostart::AutoStartConfig config = {};
    std::wstring error;
    if (!vdtrace::autostart::LoadConfig(config_path, config, error))
    {
        PrintErr(L"[fail] 读取自动启动配置失败: " + error);
        return 1;
    }

    if (restore_only)
    {
        const std::filesystem::path game_directory = std::filesystem::path(config.launch.game_path).parent_path();
        if (!RestoreAutoStartState(game_directory, error))
        {
            PrintErr(L"[fail] 还原自动启动现场失败: " + error);
            return 1;
        }

        PrintOut(L"[ok] 已清理自动启动插件、激活文件与旧代理残留。");
        return 0;
    }

    if (config.launch.game_path.empty())
    {
        PrintErr(L"[fail] launch.game_path 为空。");
        return 1;
    }

    if (!std::filesystem::exists(config.launch.game_path))
    {
        PrintErr(L"[fail] 游戏路径不存在: " + config.launch.game_path);
        return 1;
    }

    if (!std::filesystem::exists(config.launch.helper_path))
    {
        PrintErr(L"[fail] helper 路径不存在: " + config.launch.helper_path);
        return 1;
    }

    const std::wstring plugin_source = vdtrace::tools::GetExecutableDirectory() + L"\\bepinex_plugin\\VDTraceAutoStartPlugin.dll";
    std::wstring install_error;
    const std::filesystem::path game_directory = std::filesystem::path(config.launch.game_path).parent_path();
    if (!EnsureBepInExPluginInstalled(game_directory, plugin_source, install_error))
    {
        PrintErr(L"[fail] 安装 BepInEx 自动启动插件失败: " + install_error);
        return 1;
    }

    const std::filesystem::path helper_log_path = BuildHelperLogPath(config_path);
    if (!WriteActivationFile(game_directory, config_path, config.launch.helper_path, helper_log_path, install_error))
    {
        PrintErr(L"[fail] 写入自动启动激活文件失败: " + install_error);
        return 1;
    }

    STARTUPINFOW startup = {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process = {};
    std::wstring command_line = BuildCommandLine(config.launch.game_path, config.launch.arguments);
    const std::wstring working_directory = config.launch.working_directory.empty()
        ? std::filesystem::path(config.launch.game_path).parent_path().wstring()
        : config.launch.working_directory;

    const BOOL created = CreateProcessW(
        nullptr,
        command_line.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        working_directory.c_str(),
        &startup,
        &process);
    if (!created)
    {
        RemoveActivationFile(game_directory);
        PrintErr(L"[fail] 启动目标进程失败，error=" + std::to_wstring(GetLastError()));
        return 1;
    }

    CloseHandle(process.hThread);
    PrintOut(L"[start] pid=" + std::to_wstring(process.dwProcessId) + L" path=" + config.launch.game_path);
    PrintOut(L"[start] helper=" + config.launch.helper_path);
    PrintOut(L"[start] helper_log=" + helper_log_path.wstring());
    PrintOut(L"[start] BepInEx 自动启动插件已部署。");

    FILETIME launch_creation_time = {};
    FILETIME launch_exit_time = {};
    FILETIME launch_kernel_time = {};
    FILETIME launch_user_time = {};
    if (!GetProcessTimes(process.hProcess, &launch_creation_time, &launch_exit_time, &launch_kernel_time, &launch_user_time))
    {
        std::memset(&launch_creation_time, 0, sizeof(launch_creation_time));
    }

    const std::wstring normalized_game_path = std::filesystem::path(config.launch.game_path).lexically_normal().wstring();
    std::wstring last_status;
    const ULONGLONG helper_deadline = GetTickCount64() + config.wait.wait_timeout_ms + config.launch.trace_start_timeout_ms;
    DWORD active_trace_pid = 0;
    std::unordered_set<DWORD> seen_pids;
    while (GetTickCount64() < helper_deadline && active_trace_pid == 0)
    {
        const std::vector<DWORD> candidate_pids = CollectProcessesByPathSince(normalized_game_path, launch_creation_time);
        for (DWORD candidate_pid : candidate_pids)
        {
            if (seen_pids.insert(candidate_pid).second)
            {
                PrintOut(L"[start] 发现游戏进程 pid=" + std::to_wstring(candidate_pid));
            }
            if (!vdtrace::tools::WaitForPipeReady(candidate_pid, 50))
            {
                continue;
            }

            const auto status_result = vdtrace::tools::Status(candidate_pid, 200);
            if (!status_result.success)
            {
                continue;
            }

            active_trace_pid = candidate_pid;
            last_status = status_result.message;
            break;
        }

        if (active_trace_pid == 0)
        {
            Sleep(250);
        }
    }

    if (active_trace_pid == 0)
    {
        RemoveActivationFile(game_directory);
        PrintErr(L"[fail] 等待 Agent IPC 上线超时。");
        CloseHandle(process.hProcess);
        return 1;
    }

    if (process.hProcess != nullptr)
    {
        CloseHandle(process.hProcess);
    }
    process.hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, active_trace_pid);
    process.dwProcessId = active_trace_pid;
    if (process.hProcess == nullptr)
    {
        RemoveActivationFile(game_directory);
        PrintErr(L"[fail] 重新打开 Trace 目标进程失败。");
        return 1;
    }

    const ULONGLONG start_deadline = GetTickCount64() + config.wait.wait_timeout_ms + config.launch.trace_start_timeout_ms;
    bool seen_running = false;
    while (GetTickCount64() < start_deadline)
    {
        if (!IsProcessAlive(process.hProcess))
        {
            RemoveActivationFile(game_directory);
            PrintErr(L"[fail] 目标进程在 Trace 启动前退出。 " + FormatProcessExitCodeText(process.hProcess));
            CloseHandle(process.hProcess);
            return 1;
        }

        const auto status_result = vdtrace::tools::Status(process.dwProcessId, 500);
        if (status_result.success)
        {
            last_status = status_result.message;
            uint64_t running_value = 0;
            if (ParseStatusField(status_result.message, L"running=", running_value) && running_value == 1)
            {
                seen_running = true;
                PrintOut(L"[trace] 已开始: " + status_result.message);
                break;
            }
        }

        Sleep(200);
    }

    if (!seen_running)
    {
        RemoveActivationFile(game_directory);
        PrintErr(L"[fail] 等待 Trace 进入运行态超时。");
        if (!last_status.empty())
        {
            PrintErr(L"[status] " + last_status);
        }
        PrintErr(L"[hint] 查看 helper 日志: " + helper_log_path.wstring());
        CloseHandle(process.hProcess);
        return 1;
    }

    if (!config.launch.wait_for_trace_end)
    {
        RemoveActivationFile(game_directory);
        CloseHandle(process.hProcess);
        PrintOut(L"[ok] Trace 已启动，启动器退出。");
        return 0;
    }

    const ULONGLONG finish_deadline = GetTickCount64() + config.launch.trace_finish_timeout_ms;
    while (GetTickCount64() < finish_deadline)
    {
        if (!IsProcessAlive(process.hProcess))
        {
            RemoveActivationFile(game_directory);
            PrintErr(L"[fail] 目标进程在 Trace 结束前退出。 " + FormatProcessExitCodeText(process.hProcess));
            CloseHandle(process.hProcess);
            return 1;
        }

        const auto status_result = vdtrace::tools::Status(process.dwProcessId, 500);
        if (status_result.success)
        {
            last_status = status_result.message;
            uint64_t running_value = 0;
            if (ParseStatusField(status_result.message, L"running=", running_value) && running_value == 0)
            {
                RemoveActivationFile(game_directory);
                PrintOut(L"[done] Trace 已结束: " + status_result.message);
                CloseHandle(process.hProcess);
                return 0;
            }
        }

        Sleep(250);
    }

    RemoveActivationFile(game_directory);
    PrintErr(L"[fail] 等待 Trace 结束超时。");
    if (!last_status.empty())
    {
        PrintErr(L"[status] " + last_status);
    }
    PrintErr(L"[hint] 查看 helper 日志: " + helper_log_path.wstring());
    CloseHandle(process.hProcess);
    return 1;
}
