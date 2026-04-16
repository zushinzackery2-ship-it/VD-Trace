#include "session_smoke_runtime_internal.h"

namespace session_smoke
{
    namespace detail
    {
        bool BuildAnonymousExecChain()
        {
            uint8_t *buffer = static_cast<uint8_t *>(
                VirtualAlloc(nullptr, 128, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
            if (buffer == nullptr)
            {
                return false;
            }

            uint8_t *entry = buffer;
            uint8_t *nested = buffer + 0x40;
            const uintptr_t nested_address = reinterpret_cast<uintptr_t>(nested);
            const uintptr_t call_slot_address = reinterpret_cast<uintptr_t>(g_anonymous_heap_call_slot);
            const uintptr_t probe_address = reinterpret_cast<uintptr_t>(g_anonymous_heap_bytes);
            size_t offset = 0;

            const auto emit = [&](uint8_t value)
            {
                buffer[offset++] = value;
            };

            const auto emit_qword = [&](uintptr_t value)
            {
                std::memcpy(buffer + offset, &value, sizeof(value));
                offset += sizeof(value);
            };

            emit(0x48);
            emit(0x83);
            emit(0xEC);
            emit(0x28);
            emit(0x48);
            emit(0xB8);
            emit_qword(nested_address);
            emit(0xFF);
            emit(0xD0);
            emit(0x48);
            emit(0x83);
            emit(0xC4);
            emit(0x28);
            emit(0xC3);

            offset = static_cast<size_t>(nested - buffer);
            emit(0x48);
            emit(0x83);
            emit(0xEC);
            emit(0x28);
            emit(0x48);
            emit(0xB8);
            emit_qword(probe_address);
            emit(0x80);
            emit(0x00);
            emit(0x01);
            emit(0xC6);
            emit(0x40);
            emit(0x01);
            emit(0x5A);
            emit(0x80);
            emit(0x70);
            emit(0x02);
            emit(0x11);
            emit(0x48);
            emit(0xB8);
            emit_qword(call_slot_address);
            emit(0xFF);
            emit(0x10);
            emit(0x48);
            emit(0x83);
            emit(0xC4);
            emit(0x28);
            emit(0xC3);

            FlushInstructionCache(GetCurrentProcess(), buffer, 128);
            g_anonymous_exec_code = buffer;
            g_anonymous_exec_entry = reinterpret_cast<void (*)()>(entry);
            return true;
        }

        void ReleaseAnonymousExecChain()
        {
            g_anonymous_exec_entry = nullptr;
            if (g_anonymous_exec_code != nullptr)
            {
                VirtualFree(g_anonymous_exec_code, 0, MEM_RELEASE);
                g_anonymous_exec_code = nullptr;
            }
        }

        void LeafStep()
        {
            InterlockedIncrement64(&g_sink);
        }

        DWORD WINAPI AsyncSpawnedThreadMain(LPVOID)
        {
            g_async_spawned_thread_id.store(GetCurrentThreadId());
            SameLevelEntry();
            RepeatEntry();
            SetEvent(g_async_done_event);
            return 0;
        }
    }

    __declspec(noinline) void SameLevelEntry()
    {
        detail::LeafStep();
        InterlockedExchangeAdd64(&detail::g_sink, 2);
    }

    __declspec(noinline) void RepeatEntry()
    {
        for (int index = 0; index < 3; ++index)
        {
            detail::LeafStep();
        }
    }

    __declspec(noinline) void CrossModuleHelperEntry()
    {
        if (detail::g_helper_work != nullptr)
        {
            InterlockedExchangeAdd64(&detail::g_sink, static_cast<LONG64>(detail::g_helper_work(0x20u)));
        }
    }

    __declspec(noinline) void CrossModuleSystemEntry()
    {
        const DWORD tid = GetCurrentThreadId();
        detail::LeafStep();
        InterlockedExchangeAdd64(&detail::g_sink, static_cast<LONG64>(tid ^ 0x55AAu));
    }

    __declspec(noinline) void AllEventsEntry()
    {
        volatile int value = 3;
        value += 5;
        value ^= 0x11;
        if ((value & 1) != 0)
        {
            InterlockedExchangeAdd64(&detail::g_sink, value);
        }
    }

    __declspec(noinline) void HeapExtendEntry()
    {
        detail::ResetAnonymousHeapBytes();
        volatile uint8_t *buffer = detail::g_anonymous_heap_bytes;
        if (buffer == nullptr)
        {
            return;
        }

        const uint8_t first = buffer[0];
        buffer[1] = static_cast<uint8_t>(first ^ 0x5Au);
        buffer[2] = static_cast<uint8_t>(buffer[1] + 0x11u);
        detail::LeafStep();
    }

    __declspec(noinline) void StaticWindowSampleEntry()
    {
        volatile uint32_t value = 0;
        for (uint32_t index = 0; index < 8; ++index)
        {
            const uint32_t offset = (index * 29u + 3u) & 0x3FFu;
            value += static_cast<uint32_t>(detail::g_static_window_table[offset]);
        }

        InterlockedExchangeAdd64(&detail::g_sink, static_cast<LONG64>(value));
    }

    __declspec(noinline) void StaticRefEntry()
    {
        volatile uint32_t value = static_cast<uint32_t>(detail::g_static_blob[0]);
        value += static_cast<uint32_t>(detail::g_static_blob[5]);
        value ^= static_cast<uint32_t>(detail::g_static_blob[10]);
        const char *pointer = detail::g_static_text_ptr;
        value += static_cast<uint32_t>(pointer[0]);
        value ^= static_cast<uint32_t>(pointer[5]);
        InterlockedExchangeAdd64(&detail::g_sink, static_cast<LONG64>(value));
    }

    __declspec(noinline) void ProbeObservedEntry()
    {
        detail::ResetProbeBytes();
        volatile uint32_t value = static_cast<uint32_t>(detail::g_probe_bytes[0]);
        value += static_cast<uint32_t>(detail::g_probe_bytes[1]);
        value ^= static_cast<uint32_t>(detail::g_probe_bytes[2]);
        InterlockedExchangeAdd64(&detail::g_sink, static_cast<LONG64>(value));
    }

    __declspec(noinline) void AnonymousExecEntry()
    {
        detail::ResetAnonymousHeapBytes();
        if (detail::g_anonymous_exec_entry != nullptr)
        {
            detail::g_anonymous_exec_entry();
        }
    }

    __declspec(noinline) void HotLoopBody()
    {
        volatile uint32_t value = 0;
        for (uint32_t index = 0; index < 50000; ++index)
        {
            value += (index & 1u) != 0 ? 3u : 1u;
        }

        InterlockedExchangeAdd64(&detail::g_sink, static_cast<LONG64>(value));
    }

    __declspec(noinline) void HotLoopEntry()
    {
        HotLoopBody();
        detail::LeafStep();
    }

    __declspec(noinline) bool SceneHotLoopGate()
    {
        detail::LeafStep();
        return true;
    }

    __declspec(noinline) void SceneHotLoopCore()
    {
        uint64_t sum = 0;
        if (!SceneHotLoopGate())
        {
            return;
        }

        size_t count = _countof(detail::g_hot_loop_records);
        if (count != 0)
        {
            const uint32_t *cursor = &detail::g_hot_loop_records[0].value;
            do
            {
                sum += *cursor;
                cursor += 3;
                --count;
            } while (count != 0);
        }

        InterlockedExchangeAdd64(&detail::g_sink, static_cast<LONG64>(sum));
    }

    __declspec(noinline) void SceneHotLoopDepth2()
    {
        SceneHotLoopCore();
    }

    __declspec(noinline) void SceneHotLoopDepth1()
    {
        SceneHotLoopDepth2();
    }

    __declspec(noinline) void SceneHotLoopEntry()
    {
        SceneHotLoopDepth1();
    }

    __declspec(noinline) void UnityWorkerAssetEntry()
    {
        CrossModuleHelperEntry();
        SceneHotLoopDepth1();
        RepeatEntry();
    }

    __declspec(noinline) void AsyncSpawnEntry()
    {
        detail::g_async_spawned_thread_id.store(0);
        ResetEvent(detail::g_async_done_event);
        HANDLE thread = CreateThread(nullptr, 0, detail::AsyncSpawnedThreadMain, nullptr, 0, nullptr);
        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
    }
}
