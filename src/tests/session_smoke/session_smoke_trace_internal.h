#ifndef VDTRACE_SESSION_SMOKE_TRACE_INTERNAL_H
#define VDTRACE_SESSION_SMOKE_TRACE_INTERNAL_H

#include "session_smoke_support.h"

#include <Windows.h>

#include <filesystem>

namespace session_smoke
{
    namespace detail
    {
        struct WorkerContext
        {
            void (*routine)() = nullptr;
            volatile LONG ready = 0;
            volatile LONG start = 0;
        };

        struct StartSessionContext
        {
            vdtrace::Session *session = nullptr;
            volatile LONG done = 0;
            bool success = false;
            std::wstring error;
        };

        struct StopSessionContext
        {
            vdtrace::Session *session = nullptr;
            volatile LONG done = 0;
            bool success = false;
            std::wstring error;
        };

        std::wstring GetExecutablePath();
        std::filesystem::path GetExecutableDirectory();
        std::wstring GetFilenameOnly(const std::wstring &path);
        std::filesystem::path GetStaticRefsJsonPath(const std::filesystem::path &log_path);
        std::string ReadTextFile(const std::filesystem::path &path);
        void WaitForFile(const std::filesystem::path &path);
        void WaitForCaptureWaitingGrowth(vdtrace::Session &session, uint64_t baseline_count);
        DWORD WINAPI WorkerMain(LPVOID parameter);
        DWORD WINAPI StartSessionMain(LPVOID parameter);
        DWORD WINAPI StopSessionMain(LPVOID parameter);
        void ConfigureSession(
            void (*routine)(),
            const TraceRunOptions &options,
            DWORD thread_id,
            vdtrace::TextFileRecorder &recorder,
            vdtrace::Session &session);
        void LoadTraceResultFiles(const std::filesystem::path &log_path, TraceCaseResult &result);
        void WaitForSessionStop(vdtrace::Session &session, bool &auto_stopped);
    }
}

#endif
