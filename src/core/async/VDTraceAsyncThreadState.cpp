#include "pch.h"
#include "core/runtime/VDTraceInternal.h"

#include <TlHelp32.h>

namespace vdtrace
{
    namespace
    {
        bool GetThreadCreationTime(HANDLE thread_handle, FILETIME &creation_time)
        {
            FILETIME exit_time = {};
            FILETIME kernel_time = {};
            FILETIME user_time = {};
            return GetThreadTimes(thread_handle, &creation_time, &exit_time, &kernel_time, &user_time) != FALSE;
        }

        HANDLE DuplicateTracingHandle(HANDLE source_handle)
        {
            if (source_handle == nullptr || source_handle == INVALID_HANDLE_VALUE)
            {
                return nullptr;
            }

            HANDLE duplicated = nullptr;
            if (DuplicateHandle(
                    GetCurrentProcess(),
                    source_handle,
                    GetCurrentProcess(),
                    &duplicated,
                    0,
                    FALSE,
                    DUPLICATE_SAME_ACCESS))
            {
                return duplicated;
            }

            return nullptr;
        }

        bool ActivateThreadStart(Session::Impl &impl, uintptr_t entry, uintptr_t current_rip, uintptr_t current_rsp, std::wstring &error)
        {
            impl.resolved_trigger_address = entry;
            if (current_rip == entry || FindModuleRange(impl.module_ranges, current_rip) != nullptr)
            {
                impl.waiting_for_trigger = false;
                impl.root_stop_armed = impl.options.stop_on_root_return || impl.call_depth_offset != 0;
                impl.root_call_depth_base = impl.current_call_depth.load();
                if (impl.backend_mode == BackendMode::HardwareFlow)
                {
                    return StartRegionAwareHardwareFlow(impl, current_rip, nullptr, error);
                }

                impl.last_rip = current_rip;
                impl.last_rip_valid = true;
                return ArmSingleStep(impl.thread_handle, error);
            }

            uintptr_t inside_resume = 0;
            if (TryFindInsideReturnAddress(impl, current_rsp, inside_resume))
            {
                impl.resolved_trigger_address = inside_resume;
            }

            impl.waiting_for_trigger = true;
            impl.observed_addresses[0] = impl.resolved_trigger_address;
            impl.observed_addresses[1] = 0;
            impl.observed_addresses[2] = 0;
            impl.observed_addresses[3] = 0;
            impl.observed_count = 1;
            return ArmHardwareExecution(impl.thread_handle, impl.observed_addresses, impl.observed_count, error);
        }
    }

    bool TryFindNewProcessThread(Session::Impl &impl, DWORD &thread_id)
    {
        thread_id = 0;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        bool found = false;
        FILETIME newest_time = {};
        THREADENTRY32 entry = {};
        entry.dwSize = sizeof(entry);
        if (Thread32First(snapshot, &entry))
        {
            do
            {
                bool already_known = false;
                {
                    std::lock_guard<std::mutex> lock(impl.known_thread_lock);
                    already_known = impl.known_thread_ids.find(entry.th32ThreadID) != impl.known_thread_ids.end();
                }

                if (entry.th32OwnerProcessID != GetCurrentProcessId() || already_known)
                {
                    continue;
                }

                HANDLE candidate = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ThreadID);
                if (candidate == nullptr)
                {
                    continue;
                }

                FILETIME creation_time = {};
                if (GetThreadCreationTime(candidate, creation_time)
                    && (!found || CompareFileTime(&creation_time, &newest_time) > 0))
                {
                    newest_time = creation_time;
                    thread_id = entry.th32ThreadID;
                    found = true;
                }

                CloseHandle(candidate);
            } while (Thread32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return found;
    }

    bool ActivateAsyncThreadById(Session::Impl &impl, DWORD thread_id, uintptr_t entry, uint32_t depth, std::wstring &error)
    {
        const DWORD previous_thread_id = impl.active_thread_id.load();
        HANDLE trace_handle = OpenThread(
            THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION,
            FALSE,
            thread_id);
        if (trace_handle == nullptr)
        {
            error = FormatWin32Error(L"打开异步接力线程失败。", GetLastError());
            return false;
        }

        uintptr_t current_rip = 0;
        uintptr_t current_rsp = 0;
        std::wstring rip_error;
        const bool have_rip = ReadThreadControlState(trace_handle, current_rip, current_rsp, rip_error);

        if (impl.thread_handle != nullptr)
        {
            CloseHandle(impl.thread_handle);
        }

        impl.thread_handle = trace_handle;
        impl.call_depth_offset = depth + 1;
        ResetThreadObservationState(impl);
        if (!ActivateThreadStart(impl, entry, have_rip ? current_rip : 0, have_rip ? current_rsp : 0, error))
        {
            CloseHandle(impl.thread_handle);
            impl.thread_handle = nullptr;
            impl.running.store(false);
            impl.observation_state = ObservationState::Idle;
            return false;
        }

        impl.active_thread_id.store(thread_id);
        if (previous_thread_id != 0 && previous_thread_id != thread_id)
        {
            std::lock_guard<std::mutex> lock(impl.detached_thread_lock);
            impl.detached_thread_ids.insert(previous_thread_id);
        }
        {
            std::lock_guard<std::mutex> lock(impl.known_thread_lock);
            impl.known_thread_ids.insert(thread_id);
        }
        return true;
    }

    bool ActivateAsyncThreadByHandle(Session::Impl &impl, HANDLE trace_handle, uintptr_t entry, uint32_t depth, std::wstring &error)
    {
        const DWORD previous_thread_id = impl.active_thread_id.load();
        HANDLE duplicated_handle = DuplicateTracingHandle(trace_handle);
        if (duplicated_handle == nullptr || duplicated_handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        const DWORD thread_id = GetThreadId(duplicated_handle);
        if (thread_id == 0)
        {
            CloseHandle(duplicated_handle);
            return false;
        }

        uintptr_t current_rip = 0;
        std::wstring rip_error;
        const bool have_rip = ReadThreadInstructionPointer(duplicated_handle, current_rip, rip_error);

        if (impl.thread_handle != nullptr)
        {
            CloseHandle(impl.thread_handle);
        }

        impl.thread_handle = duplicated_handle;
        impl.call_depth_offset = depth + 1;
        ResetThreadObservationState(impl);
        if (!ActivateThreadStart(impl, entry, have_rip ? current_rip : 0, 0, error))
        {
            CloseHandle(impl.thread_handle);
            impl.thread_handle = nullptr;
            impl.running.store(false);
            impl.observation_state = ObservationState::Idle;
            return false;
        }

        impl.active_thread_id.store(thread_id);
        if (previous_thread_id != 0 && previous_thread_id != thread_id)
        {
            std::lock_guard<std::mutex> lock(impl.detached_thread_lock);
            impl.detached_thread_ids.insert(previous_thread_id);
        }
        {
            std::lock_guard<std::mutex> lock(impl.known_thread_lock);
            impl.known_thread_ids.insert(thread_id);
        }
        return true;
    }

    void RememberCurrentProcessThreads(Session::Impl &impl)
    {
        {
            std::lock_guard<std::mutex> lock(impl.known_thread_lock);
            impl.known_thread_ids.clear();
        }

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return;
        }

        THREADENTRY32 entry = {};
        entry.dwSize = sizeof(entry);
        if (Thread32First(snapshot, &entry))
        {
            do
            {
                if (entry.th32OwnerProcessID == GetCurrentProcessId())
                {
                    std::lock_guard<std::mutex> lock(impl.known_thread_lock);
                    impl.known_thread_ids.insert(entry.th32ThreadID);
                }
            } while (Thread32Next(snapshot, &entry));
        }

        CloseHandle(snapshot);
    }
}
