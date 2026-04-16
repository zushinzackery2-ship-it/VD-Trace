#include "pch.h"
#include "autostart/VDTraceAutoStartRuntime.h"

namespace vdtrace::autostart
{
    namespace
    {
        using Il2CppMethodGetNameFn = const char *(*)(void *);

        struct WaitState
        {
            uintptr_t invoke_address = 0;
            Il2CppMethodGetNameFn method_get_name = nullptr;
            std::string target_method_name;
            uint8_t original_byte = 0;
            HANDLE matched_event = nullptr;
            std::atomic<bool> matched = false;
            std::atomic<bool> active = false;
            std::atomic<long> step_over_count = 0;
        };

        thread_local bool g_step_over_breakpoint = false;
        thread_local bool g_rearm_breakpoint = false;
        thread_local bool g_signal_match_after_step = false;
        WaitState *g_wait_state = nullptr;

        bool ProtectAndWriteByte(uintptr_t address, uint8_t value)
        {
            DWORD old_protect = 0;
            if (!VirtualProtect(reinterpret_cast<void *>(address), 1, PAGE_EXECUTE_READWRITE, &old_protect))
            {
                return false;
            }

            *reinterpret_cast<uint8_t *>(address) = value;
            FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void *>(address), 1);

            DWORD restored_protect = 0;
            VirtualProtect(reinterpret_cast<void *>(address), 1, old_protect, &restored_protect);
            return true;
        }

        LONG CALLBACK BreakpointVectoredHandler(PEXCEPTION_POINTERS pointers)
        {
            if (pointers == nullptr || pointers->ExceptionRecord == nullptr || pointers->ContextRecord == nullptr || g_wait_state == nullptr)
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }

            if (pointers->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP && g_step_over_breakpoint)
            {
                g_step_over_breakpoint = false;
                pointers->ContextRecord->EFlags &= ~0x100u;
                if (g_rearm_breakpoint && !g_wait_state->matched.load())
                {
                    ProtectAndWriteByte(g_wait_state->invoke_address, 0xCC);
                }
                if (g_signal_match_after_step)
                {
                    g_wait_state->matched.store(true);
                    SetEvent(g_wait_state->matched_event);
                }
                g_rearm_breakpoint = false;
                g_signal_match_after_step = false;
                g_wait_state->step_over_count.fetch_sub(1);
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            if (!g_wait_state->active.load())
            {
                return EXCEPTION_CONTINUE_SEARCH;
            }

            if (pointers->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT
                && reinterpret_cast<uintptr_t>(pointers->ExceptionRecord->ExceptionAddress) == g_wait_state->invoke_address)
            {
                void *method = reinterpret_cast<void *>(static_cast<uintptr_t>(pointers->ContextRecord->Rcx));
                const char *method_name = nullptr;
                if (g_wait_state->method_get_name != nullptr)
                {
                    __try
                    {
                        method_name = g_wait_state->method_get_name(method);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER)
                    {
                        method_name = nullptr;
                    }
                }

                const bool matched = method_name != nullptr && g_wait_state->target_method_name == method_name;
                ProtectAndWriteByte(g_wait_state->invoke_address, g_wait_state->original_byte);
                g_step_over_breakpoint = true;
                g_rearm_breakpoint = !matched;
                g_signal_match_after_step = matched;
                g_wait_state->step_over_count.fetch_add(1);
                pointers->ContextRecord->Rip = g_wait_state->invoke_address;
                pointers->ContextRecord->EFlags |= 0x100u;
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            return EXCEPTION_CONTINUE_SEARCH;
        }

        bool ResolveIl2CppWaitAddresses(const AutoStartConfig &config, HMODULE &module, uintptr_t &invoke_address, Il2CppMethodGetNameFn &method_get_name, std::wstring &error)
        {
            const ULONGLONG deadline = GetTickCount64() + config.wait.wait_timeout_ms;
            while (GetTickCount64() < deadline)
            {
                module = GetModuleHandleW(config.wait.module_name.c_str());
                if (module != nullptr)
                {
                    const auto invoke_export = GetProcAddress(module, config.wait.invoke_export.c_str());
                    const auto method_name_export = GetProcAddress(module, config.wait.method_name_export.c_str());
                    if (invoke_export != nullptr && method_name_export != nullptr)
                    {
                        invoke_address = reinterpret_cast<uintptr_t>(invoke_export);
                        method_get_name = reinterpret_cast<Il2CppMethodGetNameFn>(method_name_export);
                        return true;
                    }
                }

                Sleep(config.wait.module_poll_interval_ms);
            }

            error = L"等待 IL2CPP 导出超时。";
            return false;
        }
    }

    bool WaitForConfiguredTiming(const AutoStartConfig &config, RuntimeLog &log, std::wstring &error)
    {
        error.clear();
        if (config.wait.mode == WaitMode::Disabled)
        {
            log.Append(L"wait.mode=disabled，直接进入 Agent 拉起阶段。");
            return true;
        }

        if (config.wait.mode != WaitMode::BepInExIl2CppSceneChange)
        {
            error = L"当前 helper 只支持 IL2CPP scene-change 等待模式。";
            return false;
        }

        HMODULE module = nullptr;
        uintptr_t invoke_address = 0;
        Il2CppMethodGetNameFn method_get_name = nullptr;
        if (!ResolveIl2CppWaitAddresses(config, module, invoke_address, method_get_name, error))
        {
            return false;
        }

        std::wostringstream info;
        info << L"等待时机：模块=" << config.wait.module_name.c_str()
             << L" invoke=0x" << std::hex << invoke_address << std::dec
             << L" target_method=" << std::wstring(config.wait.target_method_name.begin(), config.wait.target_method_name.end());
        log.Append(info.str());

        WaitState state = {};
        state.invoke_address = invoke_address;
        state.method_get_name = method_get_name;
        state.target_method_name = config.wait.target_method_name;
        state.matched_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (state.matched_event == nullptr)
        {
            error = L"创建 helper 等待事件失败。";
            return false;
        }

        state.original_byte = *reinterpret_cast<uint8_t *>(invoke_address);
        g_wait_state = &state;
        const PVOID handler = AddVectoredExceptionHandler(1, BreakpointVectoredHandler);
        if (handler == nullptr)
        {
            g_wait_state = nullptr;
            CloseHandle(state.matched_event);
            error = L"注册 helper VEH 失败。";
            return false;
        }

        state.active.store(true);
        if (!ProtectAndWriteByte(invoke_address, 0xCC))
        {
            state.active.store(false);
            RemoveVectoredExceptionHandler(handler);
            g_wait_state = nullptr;
            CloseHandle(state.matched_event);
            error = L"写入 IL2CPP 等待断点失败。";
            return false;
        }

        const DWORD wait_result = WaitForSingleObject(state.matched_event, config.wait.wait_timeout_ms);

        state.active.store(false);
        const ULONGLONG settle_deadline = GetTickCount64() + 1000;
        while (state.step_over_count.load() != 0 && GetTickCount64() < settle_deadline)
        {
            Sleep(1);
        }
        ProtectAndWriteByte(invoke_address, state.original_byte);
        RemoveVectoredExceptionHandler(handler);
        g_wait_state = nullptr;
        CloseHandle(state.matched_event);

        if (wait_result != WAIT_OBJECT_0 || !state.matched.load())
        {
            error = L"等待 Internal_ActiveSceneChanged 超时。";
            return false;
        }

        log.Append(L"命中 Internal_ActiveSceneChanged，开始拉起 Agent。");
        return true;
    }
}
