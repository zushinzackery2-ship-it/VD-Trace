#include "pch.h"
#include "tools/vdtrace_autostart/vdtrace_autostart_internal.h"

#include <TlHelp32.h>
#include <fstream>

namespace vdtrace::tools::autostart_cli
{
    bool TryQueryProcessPath(DWORD pid, std::wstring &path)
    {
        path.clear();
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (process == nullptr)
        {
            return false;
        }

        std::wstring buffer(1024, L'\0');
        DWORD length = static_cast<DWORD>(buffer.size());
        const BOOL queried = QueryFullProcessImageNameW(process, 0, buffer.data(), &length);
        CloseHandle(process);
        if (!queried || length == 0)
        {
            return false;
        }

        buffer.resize(length);
        path = std::filesystem::path(buffer).lexically_normal().wstring();
        return true;
    }

    bool TryQueryProcessCreationTime(DWORD pid, FILETIME &creation_time)
    {
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (process == nullptr)
        {
            return false;
        }

        FILETIME exit_time = {};
        FILETIME kernel_time = {};
        FILETIME user_time = {};
        const BOOL ok = GetProcessTimes(process, &creation_time, &exit_time, &kernel_time, &user_time);
        CloseHandle(process);
        return ok != FALSE;
    }

    std::vector<DWORD> CollectProcessesByPathSince(const std::wstring &expected_path, const FILETIME &earliest_creation_time)
    {
        std::vector<DWORD> result;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return result;
        }

        PROCESSENTRY32W entry = {};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry))
        {
            do
            {
                std::wstring process_path;
                if (!TryQueryProcessPath(entry.th32ProcessID, process_path))
                {
                    continue;
                }

                if (_wcsicmp(process_path.c_str(), expected_path.c_str()) == 0)
                {
                    FILETIME creation_time = {};
                    if (TryQueryProcessCreationTime(entry.th32ProcessID, creation_time)
                        && CompareFileTime(&creation_time, &earliest_creation_time) >= 0)
                    {
                        result.push_back(entry.th32ProcessID);
                    }
                }
            } while (Process32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return result;
    }

    bool CopyFileReplace(const std::filesystem::path &source, const std::filesystem::path &destination)
    {
        return CopyFileW(source.c_str(), destination.c_str(), FALSE) != FALSE;
    }

    std::filesystem::path AutoStartPluginInstallPath(const std::filesystem::path &game_directory)
    {
        return game_directory / L"BepInEx" / L"plugins" / L"VDTraceAutoStartPlugin.dll";
    }

    bool WriteActivationFile(
        const std::filesystem::path &game_directory,
        const std::filesystem::path &config_path,
        const std::wstring &helper_path,
        const std::filesystem::path &helper_log_path,
        std::wstring &error)
    {
        error.clear();
        const std::filesystem::path activation_path = game_directory / L"VDTraceAutoStart.activate.ini";
        std::ofstream output(activation_path, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            error = L"写入激活文件失败。";
            return false;
        }

        output << "[autostart]\n";
        output << "config_path=" << NarrowUtf8(std::filesystem::absolute(config_path).wstring()) << "\n";
        output << "helper_path=" << NarrowUtf8(helper_path) << "\n";
        output << "log_path=" << NarrowUtf8(helper_log_path.wstring()) << "\n";
        return output.good();
    }

    void RemoveActivationFile(const std::filesystem::path &game_directory)
    {
        std::error_code ec;
        std::filesystem::remove(game_directory / L"VDTraceAutoStart.activate.ini", ec);
    }

    bool RestoreAutoStartState(const std::filesystem::path &game_directory, std::wstring &error)
    {
        error.clear();
        RemoveActivationFile(game_directory);
        std::error_code ec;
        std::filesystem::remove(AutoStartPluginInstallPath(game_directory), ec);

        const std::filesystem::path active_path = game_directory / L"EndfieldBase.dll";
        const std::filesystem::path backup_path = game_directory / L"EndfieldBase_original.dll";
        if (!std::filesystem::exists(backup_path))
        {
            return true;
        }

        DeleteFileW(active_path.c_str());
        if (!MoveFileExW(backup_path.c_str(), active_path.c_str(), MOVEFILE_REPLACE_EXISTING))
        {
            error = L"恢复原始 EndfieldBase.dll 失败，error=" + std::to_wstring(GetLastError());
            return false;
        }

        return true;
    }

    bool EnsureBepInExPluginInstalled(
        const std::filesystem::path &game_directory,
        const std::filesystem::path &plugin_source,
        std::wstring &error)
    {
        error.clear();
        if (!std::filesystem::exists(plugin_source))
        {
            error = L"缺少 BepInEx 自动启动插件 DLL：";
            error += plugin_source.wstring();
            return false;
        }

        const std::filesystem::path install_path = AutoStartPluginInstallPath(game_directory);
        std::error_code ec;
        std::filesystem::create_directories(install_path.parent_path(), ec);
        if (!CopyFileReplace(plugin_source, install_path))
        {
            error = L"复制 BepInEx 自动启动插件失败，error=" + std::to_wstring(GetLastError());
            return false;
        }

        return true;
    }
}
