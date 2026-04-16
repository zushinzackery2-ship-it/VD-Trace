#include "pch.h"
#include "VDTraceTriggerCaptureInternal.h"

#include <TlHelp32.h>

namespace vdtrace::trigger_capture_detail
{
    bool GetThreadCreationTime(HANDLE thread_handle, FILETIME &creation_time)
    {
        FILETIME exit_time = {};
        FILETIME kernel_time = {};
        FILETIME user_time = {};
        return GetThreadTimes(thread_handle, &creation_time, &exit_time, &kernel_time, &user_time) != FALSE;
    }

    HANDLE OpenTracingThreadHandle(DWORD thread_id)
    {
        return OpenThread(
            THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
            FALSE,
            thread_id);
    }

    TriggerCaptureThread *FindTriggerCaptureThreadLocked(Session::Impl &impl, DWORD thread_id)
    {
        for (auto &entry : impl.trigger_capture_threads)
        {
            if (entry.thread_id == thread_id)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    bool HasTriggerCaptureThreadLocked(Session::Impl &impl, DWORD thread_id)
    {
        return FindTriggerCaptureThreadLocked(impl, thread_id) != nullptr;
    }

    bool QueueTriggerCaptureThreadLocked(Session::Impl &impl, DWORD thread_id, CONTEXT &context)
    {
        TriggerCaptureThread *entry = FindTriggerCaptureThreadLocked(impl, thread_id);
        if (entry == nullptr || entry->parked)
        {
            return false;
        }

        entry->parked = true;
        context.ContextFlags |= CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS;
        context.Dr0 = 0;
        context.Dr1 = 0;
        context.Dr2 = 0;
        context.Dr3 = 0;
        context.Dr6 = 0;
        context.Dr7 = 0;
        context.EFlags &= ~0x100u;
        return true;
    }

    bool ShouldKeepRefreshWorkerAlive(const Session::Impl &impl)
    {
        return impl.running.load() && impl.active_thread_id.load() == 0;
    }

    void ReleaseTriggerCaptureThreadsLocked(Session::Impl &impl, bool clear_state, HANDLE preserved_handle)
    {
        for (auto &entry : impl.trigger_capture_threads)
        {
            HANDLE handle = entry.handle;
            if (handle == nullptr)
            {
                continue;
            }

            if (handle != preserved_handle)
            {
                if (clear_state)
                {
                    RememberRecentCaptureThreadForStop(entry.thread_id);
                }
                CloseHandle(handle);
            }
        }

        impl.trigger_capture_threads.clear();
        impl.trigger_capture_waiting_count.store(0);
    }

    bool ArmTriggerCaptureThreads(
        Session::Impl &impl,
        bool only_new_threads,
        uint32_t &candidate_count,
        uint32_t &armed_count,
        std::wstring &error)
    {
        candidate_count = 0;
        armed_count = 0;
        error.clear();

        if (impl.configured_trigger_address == 0)
        {
            return true;
        }

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            error = L"无法枚举当前进程线程。";
            return false;
        }

        const DWORD current_thread_id = GetCurrentThreadId();
        THREADENTRY32 entry = {};
        entry.dwSize = sizeof(entry);
        uintptr_t trigger_addresses[1] = {impl.configured_trigger_address};

        if (Thread32First(snapshot, &entry))
        {
            do
            {
                if (entry.th32OwnerProcessID != GetCurrentProcessId()
                    || entry.th32ThreadID == current_thread_id
                    || entry.th32ThreadID == impl.active_thread_id.load())
                {
                    continue;
                }

                if (impl.options.block_main_thread
                    && entry.th32ThreadID == impl.main_thread_id.load())
                {
                    std::lock_guard<std::mutex> lock(impl.known_thread_lock);
                    impl.known_thread_ids.insert(entry.th32ThreadID);
                    continue;
                }

                if (only_new_threads
                    && [&impl, &entry]()
                    {
                        std::lock_guard<std::mutex> lock(impl.known_thread_lock);
                        return impl.known_thread_ids.find(entry.th32ThreadID) != impl.known_thread_ids.end();
                    }())
                {
                    continue;
                }

                bool already_captured = false;
                {
                    std::lock_guard<std::mutex> lock(impl.trigger_capture_lock);
                    already_captured = HasTriggerCaptureThreadLocked(impl, entry.th32ThreadID);
                }
                if (already_captured)
                {
                    std::lock_guard<std::mutex> lock(impl.known_thread_lock);
                    impl.known_thread_ids.insert(entry.th32ThreadID);
                    continue;
                }

                ++candidate_count;
                HANDLE thread_handle = OpenTracingThreadHandle(entry.th32ThreadID);
                if (thread_handle == nullptr)
                {
                    continue;
                }

                std::wstring arm_error;
                if (!ArmHardwareExecution(thread_handle, trigger_addresses, 1, arm_error))
                {
                    CloseHandle(thread_handle);
                    continue;
                }

                bool keep_handle = false;
                {
                    std::lock_guard<std::mutex> lock(impl.trigger_capture_lock);
                    if (!HasTriggerCaptureThreadLocked(impl, entry.th32ThreadID))
                    {
                        TriggerCaptureThread capture = {};
                        capture.thread_id = entry.th32ThreadID;
                        capture.handle = thread_handle;
                        capture.parked = false;
                        impl.trigger_capture_threads.push_back(capture);
                        impl.trigger_capture_waiting_count.store(static_cast<uint32_t>(impl.trigger_capture_threads.size()));
                        keep_handle = true;
                    }
                }

                if (!keep_handle)
                {
                    std::wstring ignored_error;
                    ClearThreadTraceState(thread_handle, ignored_error);
                    CloseHandle(thread_handle);
                    continue;
                }

                {
                    std::lock_guard<std::mutex> lock(impl.known_thread_lock);
                    impl.known_thread_ids.insert(entry.th32ThreadID);
                }
                ++armed_count;
            } while (Thread32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return true;
    }

    DWORD GetNativeThreadId(std::thread &worker)
    {
        if (!worker.joinable())
        {
            return 0;
        }

        return GetThreadId(static_cast<HANDLE>(worker.native_handle()));
    }
}
