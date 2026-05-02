#include "pch.h"
#include "VDTraceInternal.h"

namespace vdtrace
{
    namespace
    {
        constexpr int kAsyncHandoffThreadSearchAttempts = 200;
    }

    void StartAsyncHandoffWorker(Session::Impl &impl)
    {
        if (impl.async_handoff_worker.joinable())
        {
            return;
        }

        impl.async_handoff_worker_stop = false;
        impl.async_handoff_worker = std::thread(
            [&impl]()
            {
                for (;;)
                {
                    uintptr_t entry = 0;
                    uint32_t depth = 0;
                    uintptr_t handle_value = 0;
                    DWORD thread_id = 0;
                    {
                        std::unique_lock<std::mutex> lock(impl.async_handoff_lock);
                        impl.async_handoff_cv.wait(
                            lock,
                            [&impl]()
                            {
                                return impl.async_handoff_worker_stop || impl.async_handoff_request_pending;
                            });
                        if (impl.async_handoff_worker_stop)
                        {
                            break;
                        }

                        entry = impl.async_handoff_entry;
                        depth = impl.async_handoff_depth;
                        handle_value = impl.async_handoff_handle_value;
                        thread_id = impl.async_handoff_thread_id;
                        impl.async_handoff_request_pending = false;
                    }

                    Sleep(1);
                    std::wstring ignored_error;
                    if (thread_id != 0 && ActivateAsyncThreadById(impl, thread_id, entry, depth, ignored_error))
                    {
                        continue;
                    }

                    if (handle_value != 0)
                    {
                        HANDLE duplicated = reinterpret_cast<HANDLE>(handle_value);
                        if (ActivateAsyncThreadByHandle(impl, duplicated, entry, depth, ignored_error))
                        {
                            continue;
                        }
                    }

                    DWORD discovered_thread_id = 0;
                    for (int attempt = 0; attempt < kAsyncHandoffThreadSearchAttempts && impl.running.load(); attempt++)
                    {
                        if (TryFindNewProcessThread(impl, discovered_thread_id)
                            && ActivateAsyncThreadById(impl, discovered_thread_id, entry, depth, ignored_error))
                        {
                            break;
                        }
                        Sleep(1);
                    }
                }
            });

        const DWORD worker_thread_id = GetThreadId(static_cast<HANDLE>(impl.async_handoff_worker.native_handle()));
        if (worker_thread_id != 0)
        {
            std::lock_guard<std::mutex> lock(impl.known_thread_lock);
            impl.known_thread_ids.insert(worker_thread_id);
        }
    }

    void StopAsyncHandoffWorker(Session::Impl &impl)
    {
        {
            std::lock_guard<std::mutex> lock(impl.async_handoff_lock);
            impl.async_handoff_worker_stop = true;
            impl.async_handoff_request_pending = false;
        }
        impl.async_handoff_cv.notify_all();
        if (impl.async_handoff_worker.joinable())
        {
            impl.async_handoff_worker.join();
        }
    }

    void QueueAsyncHandoffRequest(Session::Impl &impl, uintptr_t entry, uint32_t depth, uintptr_t handle_value, DWORD thread_id)
    {
        {
            std::lock_guard<std::mutex> lock(impl.async_handoff_lock);
            impl.async_handoff_entry = entry;
            impl.async_handoff_depth = depth;
            impl.async_handoff_handle_value = handle_value;
            impl.async_handoff_thread_id = thread_id;
            impl.async_handoff_request_pending = true;
        }
        impl.async_handoff_cv.notify_one();
    }

    bool TryActivateAsyncThreadHandoff(
        Session::Impl &impl,
        uintptr_t async_target,
        const StepEvent &event,
        CONTEXT *context,
        std::wstring &error)
    {
        error.clear();
        if (!impl.options.async_thread_handoff)
        {
            return false;
        }

        const ResolvedAsyncProbe *probe = FindAsyncProbe(impl.async_probes, async_target);
        if (probe == nullptr || probe->thread_handle_source == ResolvedAsyncProbe::ThreadHandleSource::None)
        {
            return false;
        }

        if (probe->handoff_entry_argument_index >= event.call_argument_count
            || probe->handoff_entry_argument_index >= std::size(event.call_arguments))
        {
            return false;
        }

        const uintptr_t entry = event.call_arguments[probe->handoff_entry_argument_index];
        if (entry == 0)
        {
            return false;
        }

        HANDLE created_handle = nullptr;
        DWORD thread_id = 0;
        uintptr_t handle_value = 0;
        switch (probe->thread_handle_source)
        {
        case ResolvedAsyncProbe::ThreadHandleSource::ReturnValue:
            created_handle = reinterpret_cast<HANDLE>(context != nullptr ? context->Rax : 0);
            break;

        case ResolvedAsyncProbe::ThreadHandleSource::OutputPointerArgument:
            if (probe->thread_handle_argument_index < event.call_argument_count
                && probe->thread_handle_argument_index < std::size(event.call_arguments))
            {
                __try
                {
                    created_handle = *reinterpret_cast<HANDLE *>(event.call_arguments[probe->thread_handle_argument_index]);
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                    created_handle = nullptr;
                }
            }
            break;

        case ResolvedAsyncProbe::ThreadHandleSource::None:
        default:
            break;
        }

        if (created_handle != nullptr && created_handle != INVALID_HANDLE_VALUE)
        {
            thread_id = GetThreadId(created_handle);
            if (thread_id != 0)
            {
                handle_value = reinterpret_cast<uintptr_t>(created_handle);
            }
        }

        if (thread_id == 0
            && probe->thread_id_argument_index < event.call_argument_count
            && probe->thread_id_argument_index < std::size(event.call_arguments))
        {
            __try
            {
                thread_id = *reinterpret_cast<DWORD *>(event.call_arguments[probe->thread_id_argument_index]);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                thread_id = 0;
            }
        }

        if (thread_id == 0 && handle_value == 0)
        {
            return false;
        }

        QueueAsyncHandoffRequest(impl, entry, event.call_depth, handle_value, thread_id);
        if (context != nullptr && impl.active_thread_id.load() == GetCurrentThreadId())
        {
            uintptr_t empty[4] = {};
            ApplyHardwareContextObservations(*context, empty, 0, false);
        }
        impl.observation_state = ObservationState::Idle;
        return true;
    }
}
