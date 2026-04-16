#pragma once

#include <Windows.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "loader/VDTraceLoaderIpc.h"

namespace VDTraceLoader
{
    inline std::filesystem::path gRuntimeLogPath;
    inline std::atomic<bool> gStopRequested = false;
    inline std::atomic<bool> gControllerRuntimeActive = false;
    inline std::mutex gRuntimeLogMutex;

    inline std::string Narrow(const std::wstring &text)
    {
        if (text.empty())
        {
            return {};
        }

        const auto count = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (count <= 1)
        {
            return {};
        }

        std::string result(static_cast<std::size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), count, nullptr, nullptr);
        if (!result.empty() && result.back() == '\0')
        {
            result.pop_back();
        }
        return result;
    }

    inline std::wstring GetProcessPath()
    {
        std::wstring buffer(MAX_PATH, L'\0');
        const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return {};
        }

        buffer.resize(length);
        return buffer;
    }

    inline bool EqualsInsensitive(std::wstring_view left, std::wstring_view right)
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            if (::towlower(left[index]) != ::towlower(right[index]))
            {
                return false;
            }
        }

        return true;
    }

    inline bool IsAllowedControllerProcess()
    {
        const auto processPath = GetProcessPath();
        if (processPath.empty())
        {
            return false;
        }

        const auto fileName = std::filesystem::path(processPath).filename().wstring();
        return EqualsInsensitive(fileName, L"Endfield.exe");
    }

    inline std::filesystem::path GetModulePath(HMODULE module)
    {
        std::wstring buffer(MAX_PATH, L'\0');
        const auto length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return {};
        }

        buffer.resize(length);
        return std::filesystem::path(buffer);
    }

    inline void AppendRuntimeLog(const std::string &line)
    {
        std::lock_guard<std::mutex> lock(gRuntimeLogMutex);
        if (gRuntimeLogPath.empty())
        {
            return;
        }

        std::ofstream out(gRuntimeLogPath, std::ios::app | std::ios::binary);
        if (!out)
        {
            return;
        }

        out << "[VDTraceLoader] " << line << "\n";
    }

    inline void CloseClientPipe(HANDLE &pipeHandle)
    {
        if (pipeHandle == nullptr || pipeHandle == INVALID_HANDLE_VALUE)
        {
            pipeHandle = INVALID_HANDLE_VALUE;
            return;
        }

        CloseHandle(pipeHandle);
        pipeHandle = INVALID_HANDLE_VALUE;
    }

    inline bool SendControllerLog(HANDLE pipeHandle, const char *text)
    {
        return VDTraceLoaderIpc::SendAgentLog(pipeHandle, GetCurrentProcessId(), text);
    }

    inline bool HandleLoadDllRequest(HANDLE pipeHandle, const std::vector<std::uint8_t> &buffer)
    {
        if (buffer.size() < sizeof(VDTraceLoaderIpc::MessageHeader) + sizeof(VDTraceLoaderIpc::LoadDllRequestPayload))
        {
            return true;
        }

        const auto *payload = reinterpret_cast<const VDTraceLoaderIpc::LoadDllRequestPayload *>(buffer.data() + sizeof(VDTraceLoaderIpc::MessageHeader));
        const std::wstring dllPath(payload->dllPath);
        AppendRuntimeLog("Controller requested DLL = " + Narrow(dllPath));
        if (dllPath.empty())
        {
            VDTraceLoaderIpc::SendLoadDllReply(pipeHandle, GetCurrentProcessId(), 1, ERROR_INVALID_PARAMETER, dllPath, L"dll path is empty");
            return true;
        }

        const auto dllHandle = LoadLibraryW(dllPath.c_str());
        if (dllHandle == nullptr)
        {
            const auto error = GetLastError();
            AppendRuntimeLog("LoadLibraryW(selected DLL) failed, error = " + std::to_string(error));
            VDTraceLoaderIpc::SendLoadDllReply(pipeHandle, GetCurrentProcessId(), 1, error, dllPath, L"LoadLibraryW failed");
            return true;
        }

        using VdTraceBootstrapFn = BOOL(WINAPI *)();
        const auto bootstrap = reinterpret_cast<VdTraceBootstrapFn>(GetProcAddress(dllHandle, "vdtrace_loader_bootstrap"));
        if (bootstrap == nullptr)
        {
            AppendRuntimeLog("vdtrace_loader_bootstrap export not found");
            VDTraceLoaderIpc::SendLoadDllReply(pipeHandle, GetCurrentProcessId(), 1, ERROR_PROC_NOT_FOUND, dllPath, L"bootstrap export not found");
            FreeLibrary(dllHandle);
            return true;
        }

        if (!bootstrap())
        {
            AppendRuntimeLog("vdtrace_loader_bootstrap failed");
            VDTraceLoaderIpc::SendLoadDllReply(pipeHandle, GetCurrentProcessId(), 1, ERROR_GEN_FAILURE, dllPath, L"bootstrap failed");
            FreeLibrary(dllHandle);
            return true;
        }

        AppendRuntimeLog("Loaded DLL from " + Narrow(dllPath));
        VDTraceLoaderIpc::SendLoadDllReply(pipeHandle, GetCurrentProcessId(), 0, 0, dllPath, L"ok");
        return true;
    }

    inline DWORD WINAPI ControllerThread(void *)
    {
        AppendRuntimeLog("Controller thread entered");
        AppendRuntimeLog("PID = " + std::to_string(GetCurrentProcessId()));
        AppendRuntimeLog("Process = " + Narrow(GetProcessPath()));

        DWORD lastWaitLogTick = 0;
        while (!gStopRequested.load())
        {
            HANDLE pipeHandle = VDTraceLoaderIpc::ConnectToPipe(500);
            if (pipeHandle == INVALID_HANDLE_VALUE)
            {
                const auto now = GetTickCount();
                if (lastWaitLogTick == 0 || now - lastWaitLogTick >= 5000)
                {
                    AppendRuntimeLog("Waiting for controller pipe");
                    lastWaitLogTick = now;
                }

                Sleep(1000);
                continue;
            }

            lastWaitLogTick = 0;
            AppendRuntimeLog("Controller connected");
            VDTraceLoaderIpc::SendAgentHello(pipeHandle, GetCurrentProcessId(), GetProcessPath());
            SendControllerLog(pipeHandle, "agent connected");

            std::vector<std::uint8_t> buffer;
            while (!gStopRequested.load() && VDTraceLoaderIpc::ReadMessage(pipeHandle, buffer))
            {
                if (buffer.size() < sizeof(VDTraceLoaderIpc::MessageHeader))
                {
                    continue;
                }

                const auto *header = reinterpret_cast<const VDTraceLoaderIpc::MessageHeader *>(buffer.data());
                const auto kind = static_cast<VDTraceLoaderIpc::MessageKind>(header->kind);
                if (kind == VDTraceLoaderIpc::MessageKind::LoadDllRequest)
                {
                    HandleLoadDllRequest(pipeHandle, buffer);
                }
            }

            AppendRuntimeLog("Controller disconnected");
            CloseClientPipe(pipeHandle);
            Sleep(1000);
        }

        AppendRuntimeLog("Controller thread exiting");
        return 0;
    }

    inline BOOL HandleDllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
    {
        (void) reserved;

        if (reason == DLL_PROCESS_ATTACH)
        {
            DisableThreadLibraryCalls(instance);
            gStopRequested = false;

            if (!IsAllowedControllerProcess())
            {
                return TRUE;
            }

            const auto modulePath = GetModulePath(instance);
            gRuntimeLogPath = modulePath.parent_path() / L"vdtrace_loader_runtime_log.txt";
            AppendRuntimeLog("DllMain attach");

            const auto thread = CreateThread(nullptr, 0, &ControllerThread, nullptr, 0, nullptr);
            if (thread == nullptr)
            {
                AppendRuntimeLog("CreateThread failed, error = " + std::to_string(GetLastError()));
                return TRUE;
            }

            gControllerRuntimeActive = true;
            CloseHandle(thread);
        }
        else if (reason == DLL_PROCESS_DETACH)
        {
            if (gControllerRuntimeActive.load())
            {
                gStopRequested = true;
                gControllerRuntimeActive = false;
            }
        }

        return TRUE;
    }
}
