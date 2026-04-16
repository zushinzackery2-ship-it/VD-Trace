#include "session_smoke_runtime_internal.h"

namespace session_smoke
{
    namespace detail
    {
        volatile LONG64 g_sink = 0;
        HelperWorkFn g_helper_work = nullptr;
        HMODULE g_helper_module = nullptr;
        alignas(16) uint8_t g_probe_bytes[32] = {
            0x3A, 0xF1, 0x8C, 0x47, 0xB2, 0x09, 0x6D, 0xEE,
            0x51, 0x24, 0x90, 0x7C, 0x18, 0xD3, 0xA4, 0x62,
            0xC7, 0x12, 0x5E, 0xA9, 0x04, 0xDB, 0x33, 0x88,
            0xF2, 0x0E, 0x77, 0x49, 0x65, 0xBA, 0x1C, 0x93,
        };
        alignas(16) const uint8_t g_probe_bytes_seed[32] = {
            0x3A, 0xF1, 0x8C, 0x47, 0xB2, 0x09, 0x6D, 0xEE,
            0x51, 0x24, 0x90, 0x7C, 0x18, 0xD3, 0xA4, 0x62,
            0xC7, 0x12, 0x5E, 0xA9, 0x04, 0xDB, 0x33, 0x88,
            0xF2, 0x0E, 0x77, 0x49, 0x65, 0xBA, 0x1C, 0x93,
        };
        alignas(16) const uint8_t g_static_blob[16] = {
            0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
            0x13, 0x57, 0x9B, 0xDF, 0x24, 0x68, 0xAC, 0xE0,
        };
        alignas(64) const uint8_t g_static_window_table[1024] = {
            0x51, 0xA2, 0x1C, 0x7F, 0x33, 0xD8, 0x44, 0x90,
            0x62, 0x10, 0xEF, 0x2B, 0x18, 0x74, 0xC1, 0x5A,
            0x8E, 0x04, 0x39, 0xB7, 0xCD, 0x26, 0xF0, 0x11,
            0x57, 0x9A, 0x6D, 0x03, 0xE4, 0x28, 0x7B, 0xC6,
        };
        const char g_static_text[] = "vdtrace-static-pointer";
        const char *g_static_text_ptr = g_static_text;
        HotLoopRecord g_hot_loop_records[50000] = {};
        UnityWorkerSlot g_unity_workers[2] = {};
        HANDLE g_async_done_event = nullptr;
        std::atomic<DWORD> g_async_spawned_thread_id = 0;
        uint8_t *g_anonymous_exec_code = nullptr;
        void (*g_anonymous_exec_entry)() = nullptr;
        uint8_t *g_anonymous_heap_bytes = nullptr;
        uintptr_t *g_anonymous_heap_call_slot = nullptr;

        bool AllocateAnonymousHeapState()
        {
            g_anonymous_heap_bytes = static_cast<uint8_t *>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, 32));
            if (g_anonymous_heap_bytes == nullptr)
            {
                return false;
            }

            g_anonymous_heap_call_slot = static_cast<uintptr_t *>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(uintptr_t)));
            if (g_anonymous_heap_call_slot == nullptr)
            {
                HeapFree(GetProcessHeap(), 0, g_anonymous_heap_bytes);
                g_anonymous_heap_bytes = nullptr;
                return false;
            }

            std::memcpy(g_anonymous_heap_bytes, g_probe_bytes_seed, sizeof(g_probe_bytes_seed));
            *g_anonymous_heap_call_slot = reinterpret_cast<uintptr_t>(&LeafStep);
            return true;
        }

        void ReleaseAnonymousHeapState()
        {
            if (g_anonymous_heap_call_slot != nullptr)
            {
                HeapFree(GetProcessHeap(), 0, g_anonymous_heap_call_slot);
                g_anonymous_heap_call_slot = nullptr;
            }

            if (g_anonymous_heap_bytes != nullptr)
            {
                HeapFree(GetProcessHeap(), 0, g_anonymous_heap_bytes);
                g_anonymous_heap_bytes = nullptr;
            }
        }

        std::filesystem::path GetRuntimeExecutableDirectory()
        {
            std::wstring buffer(MAX_PATH, L'\0');
            DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            while (length == buffer.size())
            {
                buffer.resize(buffer.size() * 2);
                length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            }

            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }

        DWORD WINAPI UnityWorkerMain(LPVOID parameter)
        {
            auto *slot = static_cast<UnityWorkerSlot *>(parameter);
            slot->thread_id = GetCurrentThreadId();
            SetEvent(slot->ready_event);
            for (;;)
            {
                WaitForSingleObject(slot->dispatch_event, INFINITE);
                ResetEvent(slot->dispatch_event);
                if (InterlockedCompareExchange(&slot->stop, 0, 0) != 0)
                {
                    break;
                }

                void (*routine)() = slot->routine;
                if (routine != nullptr)
                {
                    routine();
                }
                SetEvent(slot->done_event);
            }
            return 0;
        }

        void StartUnityWorkers()
        {
            for (size_t index = 0; index < _countof(g_unity_workers); ++index)
            {
                UnityWorkerSlot &slot = g_unity_workers[index];
                slot.ready_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                slot.dispatch_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                slot.done_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                Require(slot.ready_event != nullptr && slot.dispatch_event != nullptr && slot.done_event != nullptr, "failed to create unity worker events");
                slot.thread = CreateThread(nullptr, 0, UnityWorkerMain, &slot, 0, nullptr);
                Require(slot.thread != nullptr, "failed to create unity worker");
            }

            HANDLE ready_events[_countof(g_unity_workers)] = {g_unity_workers[0].ready_event, g_unity_workers[1].ready_event};
            Require(WaitForMultipleObjects(static_cast<DWORD>(_countof(ready_events)), ready_events, TRUE, 3000) == WAIT_OBJECT_0, "unity workers did not become ready");
        }

        void StopUnityWorkers()
        {
            for (auto &slot : g_unity_workers)
            {
                if (slot.thread == nullptr)
                {
                    continue;
                }
                InterlockedExchange(&slot.stop, 1);
                SetEvent(slot.dispatch_event);
            }

            for (auto &slot : g_unity_workers)
            {
                if (slot.thread != nullptr)
                {
                    WaitForSingleObject(slot.thread, 3000);
                    CloseHandle(slot.thread);
                    CloseHandle(slot.ready_event);
                    CloseHandle(slot.dispatch_event);
                    CloseHandle(slot.done_event);
                    slot = {};
                }
            }
        }

        void DispatchUnityWorker(size_t index, void (*routine)())
        {
            Require(index < _countof(g_unity_workers), "invalid unity worker index");
            UnityWorkerSlot &slot = g_unity_workers[index];
            ResetEvent(slot.done_event);
            slot.routine = routine;
            SetEvent(slot.dispatch_event);
        }

        void ResetProbeBytes()
        {
            std::memcpy(g_probe_bytes, g_probe_bytes_seed, sizeof(g_probe_bytes));
        }

        void ResetAnonymousHeapBytes()
        {
            Require(g_anonymous_heap_bytes != nullptr, "anonymous heap buffer missing");
            Require(g_anonymous_heap_call_slot != nullptr, "anonymous heap call slot missing");
            std::memcpy(g_anonymous_heap_bytes, g_probe_bytes_seed, sizeof(g_probe_bytes_seed));
            *g_anonymous_heap_call_slot = reinterpret_cast<uintptr_t>(&LeafStep);
        }
    }

    void InitializeEnvironment()
    {
        SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

        for (size_t index = 0; index < _countof(detail::g_hot_loop_records); ++index)
        {
            detail::g_hot_loop_records[index].pad0 = static_cast<uint32_t>(index ^ 0x1357u);
            detail::g_hot_loop_records[index].value = static_cast<uint32_t>((index * 3u) ^ 0x55AA1234u);
            detail::g_hot_loop_records[index].pad1 = static_cast<uint32_t>(index + 7u);
        }

        detail::g_async_done_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Require(detail::g_async_done_event != nullptr, "failed to create async done event");
        detail::g_helper_module = LoadLibraryW((detail::GetRuntimeExecutableDirectory() / L"VDTraceTriggerWaitHelper.dll").c_str());
        Require(detail::g_helper_module != nullptr, "failed to load VDTraceTriggerWaitHelper.dll");
        detail::g_helper_work = reinterpret_cast<detail::HelperWorkFn>(GetProcAddress(detail::g_helper_module, "TriggerWaitHelperWork"));
        Require(detail::g_helper_work != nullptr, "failed to resolve helper export");
        Require(detail::AllocateAnonymousHeapState(), "failed to allocate anonymous heap state");
        Require(detail::BuildAnonymousExecChain(), "failed to build anonymous executable test chain");
        detail::StartUnityWorkers();
    }

    void ShutdownEnvironment()
    {
        detail::StopUnityWorkers();
        detail::ReleaseAnonymousExecChain();
        detail::ReleaseAnonymousHeapState();
        if (detail::g_helper_module != nullptr)
        {
            FreeLibrary(detail::g_helper_module);
            detail::g_helper_module = nullptr;
            detail::g_helper_work = nullptr;
        }
        if (detail::g_async_done_event != nullptr)
        {
            CloseHandle(detail::g_async_done_event);
            detail::g_async_done_event = nullptr;
        }
    }

    void DispatchUnityWorkerTask(size_t index, void (*routine)())
    {
        detail::DispatchUnityWorker(index, routine);
    }

    void WaitForUnityWorkerTask(size_t index)
    {
        Require(index < _countof(detail::g_unity_workers), "invalid unity worker index");
        Require(WaitForSingleObject(detail::g_unity_workers[index].done_event, 10000) == WAIT_OBJECT_0, "unity worker task did not complete");
    }

    DWORD UnityWorkerThreadId(size_t index)
    {
        Require(index < _countof(detail::g_unity_workers), "invalid unity worker index");
        return detail::g_unity_workers[index].thread_id;
    }

    DWORD AsyncSpawnedThreadId()
    {
        return detail::g_async_spawned_thread_id.load();
    }

    uintptr_t ProbeObservedMidpoint()
    {
        const uintptr_t entry = reinterpret_cast<uintptr_t>(&ProbeObservedEntry);
        const vdtrace::InstructionDecodeResult first = vdtrace::DecodeInstruction(entry);
        Require(first.size != 0, "failed to decode probe entry");
        return entry + first.size;
    }

    uintptr_t ProbeBytesAddress()
    {
        return reinterpret_cast<uintptr_t>(detail::g_probe_bytes);
    }

    uintptr_t AnonymousHeapBytesAddress()
    {
        return reinterpret_cast<uintptr_t>(detail::g_anonymous_heap_bytes);
    }

    uintptr_t AnonymousExecCodeAddress()
    {
        return reinterpret_cast<uintptr_t>(detail::g_anonymous_exec_entry);
    }

    void WaitForAsyncWorkerDone()
    {
        Require(WaitForSingleObject(detail::g_async_done_event, 10000) == WAIT_OBJECT_0, "async worker did not complete");
    }
}
