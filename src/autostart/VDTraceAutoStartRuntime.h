#ifndef VDTRACE_AUTOSTART_RUNTIME_H
#define VDTRACE_AUTOSTART_RUNTIME_H

#include "autostart/VDTraceAutoStartConfig.h"

#include <filesystem>
#include <mutex>
#include <string>

namespace vdtrace::autostart
{
    class RuntimeLog
    {
    public:
        explicit RuntimeLog(std::filesystem::path path);

        void Append(const std::wstring &text);
        void AppendAnsi(const std::string &text);
        const std::filesystem::path &Path() const;

    private:
        std::filesystem::path path_;
        mutable std::mutex lock_;
    };

    std::filesystem::path ReadAutoStartConfigPathFromEnvironment();
    std::filesystem::path ReadAutoStartLogPathFromEnvironment();
    bool WaitForConfiguredTiming(const AutoStartConfig &config, RuntimeLog &log, std::wstring &error);
}

#endif
