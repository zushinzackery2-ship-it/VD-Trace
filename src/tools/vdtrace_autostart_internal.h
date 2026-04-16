#ifndef VDTRACE_AUTOSTART_INTERNAL_H
#define VDTRACE_AUTOSTART_INTERNAL_H

#include "autostart/VDTraceAutoStartConfig.h"

#include <Windows.h>

#include <filesystem>
#include <string>
#include <vector>

namespace vdtrace::tools::autostart_cli
{
    std::string NarrowUtf8(const std::wstring &text);
    void WriteText(HANDLE handle, const std::wstring &text);
    void PrintOut(const std::wstring &text);
    void PrintErr(const std::wstring &text);
    void PrintUsage();
    std::wstring BuildTimestampSuffix();
    bool ParseStatusField(const std::wstring &text, const std::wstring &key, uint64_t &value);
    std::wstring BuildCommandLine(const std::wstring &game_path, const std::wstring &arguments);
    std::filesystem::path BuildHelperLogPath(const std::filesystem::path &config_path);
    bool IsProcessAlive(HANDLE process);
    bool TryGetProcessExitCodeValue(HANDLE process, DWORD &exit_code);
    std::wstring FormatProcessExitCodeText(HANDLE process);
    bool TryQueryProcessPath(DWORD pid, std::wstring &path);
    bool TryQueryProcessCreationTime(DWORD pid, FILETIME &creation_time);
    std::vector<DWORD> CollectProcessesByPathSince(const std::wstring &expected_path, const FILETIME &earliest_creation_time);
    bool CopyFileReplace(const std::filesystem::path &source, const std::filesystem::path &destination);
    std::filesystem::path AutoStartPluginInstallPath(const std::filesystem::path &game_directory);
    bool WriteActivationFile(
        const std::filesystem::path &game_directory,
        const std::filesystem::path &config_path,
        const std::wstring &helper_path,
        const std::filesystem::path &helper_log_path,
        std::wstring &error);
    void RemoveActivationFile(const std::filesystem::path &game_directory);
    bool RestoreAutoStartState(const std::filesystem::path &game_directory, std::wstring &error);
    bool EnsureBepInExPluginInstalled(
        const std::filesystem::path &game_directory,
        const std::filesystem::path &plugin_source,
        std::wstring &error);
}

#endif
