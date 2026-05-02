#include "pch.h"
#include "VDTraceInternal.h"
#include "VDTraceRuntimeInternal.h"

namespace vdtrace
{
    namespace
    {
        constexpr DWORD kSlowTraceStopTimeoutMs = 2000;
    }

    bool Session::Impl::Start(std::wstring &error)
    {
        std::lock_guard<std::mutex> lock(state_lock);
        if (!configured.load())
        {
            error = L"trace 尚未配置。";
            return false;
        }

        if (running.load())
        {
            error = L"trace 已在运行。";
            return false;
        }

        if (thread_handle != nullptr)
        {
            CloseHandle(thread_handle);
            thread_handle = nullptr;
        }

        StopTriggerCaptureRefreshWorker(*this);
        ReleaseTriggerCaptureThreads(*this, true);

        if (veh_handle == nullptr)
        {
            veh_handle = AddVectoredExceptionHandler(1, runtime_detail::GlobalVectoredHandler);
            if (veh_handle == nullptr)
            {
                error = FormatWin32Error(L"注册 VEH 失败。", GetLastError());
                return false;
            }
        }

        runtime_detail::ActivateRuntimeSession(this);
        runtime_detail::ResetRecentStopState();

        runtime_detail::ResetExecutionState(*this);
        active_thread_id.store(options.thread_id);
        RememberCurrentProcessThreads(*this);
        ClearExecutionSummary(*this);
        if (options.async_thread_handoff)
        {
            StartAsyncHandoffWorker(*this);
        }
        running.store(true);

        if (resolved_trigger_address != 0)
        {
            if (options.auto_select_thread)
            {
                active_thread_id.store(0);
                if (!BeginTriggerThreadCapture(*this, error))
                {
                    running.store(false);
                    runtime_detail::DeactivateRuntimeSession(this);
                    return false;
                }
                return true;
            }

            thread_handle = OpenThread(
                THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
                FALSE,
                options.thread_id);
            if (thread_handle == nullptr)
            {
                error = FormatWin32Error(L"打开目标线程失败。", GetLastError());
                running.store(false);
                runtime_detail::DeactivateRuntimeSession(this);
                return false;
            }

            waiting_for_trigger = true;
            observed_addresses[0] = resolved_trigger_address;
            observed_count = 1;
            if (!ArmHardwareExecution(thread_handle, observed_addresses, observed_count, error))
            {
                running.store(false);
                runtime_detail::DeactivateRuntimeSession(this);
                CloseHandle(thread_handle);
                thread_handle = nullptr;
                return false;
            }
            return true;
        }

        runtime_detail::ArmRootStop(*this);

        thread_handle = OpenThread(
            THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
            FALSE,
            options.thread_id);
        if (thread_handle == nullptr)
        {
            error = FormatWin32Error(L"打开目标线程失败。", GetLastError());
            running.store(false);
            runtime_detail::DeactivateRuntimeSession(this);
            return false;
        }

        if (backend_mode == BackendMode::HardwareFlow)
        {
            uintptr_t rip = 0;
            if (!ReadThreadInstructionPointer(thread_handle, rip, error))
            {
                running.store(false);
                runtime_detail::DeactivateRuntimeSession(this);
                CloseHandle(thread_handle);
                thread_handle = nullptr;
                return false;
            }

            if (!StartRegionAwareHardwareFlow(*this, rip, nullptr, error))
            {
                running.store(false);
                runtime_detail::DeactivateRuntimeSession(this);
                CloseHandle(thread_handle);
                thread_handle = nullptr;
                return false;
            }
        }
        else if (backend_mode == BackendMode::TrapFlagContext)
        {
            if (!ReadThreadInstructionPointer(thread_handle, last_rip, error) || !ArmSingleStep(thread_handle, error))
            {
                running.store(false);
                runtime_detail::DeactivateRuntimeSession(this);
                CloseHandle(thread_handle);
                thread_handle = nullptr;
                return false;
            }

            last_rip_valid = true;
        }
        return error.empty();
    }

    bool Session::Impl::Stop(std::wstring &error)
    {
        std::lock_guard<std::mutex> lock(state_lock);
        error.clear();
        running.store(false);
        StopAsyncHandoffWorker(*this);
        StopTriggerCaptureRefreshWorker(*this);
        ReleaseTriggerCaptureThreads(*this, true);

        if (thread_handle != nullptr)
        {
            runtime_detail::MarkRecentStopThread(active_thread_id.load(), GetTickCount64() + kSlowTraceStopTimeoutMs);
            if (backend_mode == BackendMode::TrapFlagContext)
            {
                stop_requested.store(true);
                const ULONGLONG begin = GetTickCount64();
                while (running.load() && GetTickCount64() - begin < kSlowTraceStopTimeoutMs)
                {
                    Sleep(1);
                }
            }
            else
            {
                stop_requested.store(false);
            }

            std::wstring clear_error;
            ClearThreadTraceState(thread_handle, clear_error);
            CloseHandle(thread_handle);
            thread_handle = nullptr;
        }

        while (active_handler_count.load(std::memory_order_acquire) > 0)
        {
            Sleep(0);
        }

        SnapshotExecutionSummary(*this);
        runtime_detail::ResetExecutionState(*this);
        runtime_detail::DeactivateRuntimeSession(this);

        return error.empty();
    }
}
