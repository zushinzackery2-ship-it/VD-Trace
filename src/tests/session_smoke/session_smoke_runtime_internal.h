#ifndef VDTRACE_SESSION_SMOKE_RUNTIME_INTERNAL_H
#define VDTRACE_SESSION_SMOKE_RUNTIME_INTERNAL_H

#include "session_smoke_support.h"
#include "core/runtime/VDTraceInternal.h"

#include <Windows.h>

#include <atomic>
#include <filesystem>

namespace session_smoke
{
    namespace detail
    {
        using HelperWorkFn = uint32_t(__stdcall *)(uint32_t);

        struct HotLoopRecord
        {
            uint32_t pad0;
            uint32_t value;
            uint32_t pad1;
        };

        struct UnityWorkerSlot
        {
            HANDLE thread = nullptr;
            HANDLE ready_event = nullptr;
            HANDLE dispatch_event = nullptr;
            HANDLE done_event = nullptr;
            void (*routine)() = nullptr;
            volatile LONG stop = 0;
            DWORD thread_id = 0;
        };

        extern volatile LONG64 g_sink;
        extern HelperWorkFn g_helper_work;
        extern HMODULE g_helper_module;
        extern alignas(16) uint8_t g_probe_bytes[32];
        extern alignas(16) const uint8_t g_probe_bytes_seed[32];
        extern alignas(16) const uint8_t g_static_blob[16];
        extern alignas(64) const uint8_t g_static_window_table[1024];
        extern const char g_static_text[];
        extern const char *g_static_text_ptr;
        extern HotLoopRecord g_hot_loop_records[50000];
        extern UnityWorkerSlot g_unity_workers[2];
        extern HANDLE g_async_done_event;
        extern std::atomic<DWORD> g_async_spawned_thread_id;
        extern uint8_t *g_anonymous_exec_code;
        extern void (*g_anonymous_exec_entry)();
        extern uint8_t *g_anonymous_heap_bytes;
        extern uintptr_t *g_anonymous_heap_call_slot;

        std::filesystem::path GetRuntimeExecutableDirectory();
        bool AllocateAnonymousHeapState();
        void ReleaseAnonymousHeapState();
        bool BuildAnonymousExecChain();
        void ReleaseAnonymousExecChain();
        void StartUnityWorkers();
        void StopUnityWorkers();
        void DispatchUnityWorker(size_t index, void (*routine)());
        void ResetProbeBytes();
        void ResetAnonymousHeapBytes();
        void LeafStep();
    }
}

#endif
