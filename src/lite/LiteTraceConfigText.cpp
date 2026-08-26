#include "pch.h"
#include "lite/LiteTraceConfig.h"

namespace vdtrace::lite
{
    namespace
    {
        std::filesystem::path ModuleDirectoryFromAddress(const void *address)
        {
            HMODULE module = nullptr;
            if (!GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(address),
                    &module))
            {
                return std::filesystem::current_path();
            }

            std::wstring buffer(MAX_PATH, L'\0');
            DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
            while (length == buffer.size())
            {
                buffer.resize(buffer.size() * 2);
                length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
            }

            if (length == 0)
            {
                return std::filesystem::current_path();
            }

            buffer.resize(length);
            return std::filesystem::path(buffer).parent_path();
        }
    }

    std::filesystem::path LiteTraceModuleDirectory()
    {
        return ModuleDirectoryFromAddress(reinterpret_cast<const void *>(&LiteTraceModuleDirectory));
    }

    std::filesystem::path DefaultLiteTraceConfigPath()
    {
        const std::filesystem::path working_directory_path = std::filesystem::current_path() / L"LiteTrace.ini";
        if (std::filesystem::exists(working_directory_path))
        {
            return working_directory_path.lexically_normal();
        }

        const std::filesystem::path module_directory_path = LiteTraceModuleDirectory() / L"LiteTrace.ini";
        return module_directory_path.lexically_normal();
    }

    std::string BuildDefaultLiteTraceConfigText()
    {
        return
            "; LiteTrace lightweight in-process tracer\n"
            "; Inject LiteTrace.dll into the target process: it reads this file (current\n"
            "; working directory first, then the LiteTrace.dll folder), installs the\n"
            "; [trace] trigger_point breakpoint, waits for it to fire, records the trace,\n"
            "; and can end the process afterwards.\n"
            "; Core is reused from VD-Trace (VDTraceStatic Session/Options); no Agent or\n"
            "; IPC is involved, so this is lighter than VDTraceAutoStart.\n"
            "; Pure trigger tracer: whichever thread reaches trigger_point is traced. There\n"
            "; is no thread pinning / rotation / cross-thread handoff (those are AutoStart /\n"
            "; Agent features), so there is no thread_id or auto_select_thread key here.\n"
            "; backend supports DR / TF only. call_depth uses single / all / number.\n"
            "; outside_execution_mode / anonymous_exec_execution_mode support EDGE or TF.\n"
            "; module_call_depths format uses Module.dll:3:TF entries separated by commas.\n"
            "; trigger_point / end_point format: Module.dll+0xRVA or Module.dll!0xRVA or\n"
            "; 0xABSOLUTE.\n"
            "; sim_fast_forward skips single-step exceptions on deterministic direct jumps\n"
            "; (DR backend only). sim_fast_forward_indirect additionally resolves register\n"
            "; jumps from a pure register computation - Windows validation recommended.\n"
            ";\n"
            "; Two modes (set lite.mode below):\n"
            ";   step      : trace from trigger_point for max_events steps; end_point and\n"
            ";               root_stop_on_return are ignored.\n"
            ";   specified : trace from trigger_point until end_point is reached (max_events\n"
            ";               is forced to 0 = unlimited); the step count is ignored.\n"
            "\n"
            "[trace]\n"
            "modules = \n"
            "output_path = .\\traces\\LiteTrace.log\n"
            "trigger_point = \n"
            "trigger_enabled = true\n"
            "; end_point is only used in specified mode; execution stops when it is reached.\n"
            "end_point = \n"
            "probe_spec = \n"
            "max_events = 20000\n"
            "backend = DR\n"
            "all_events = false\n"
            "idle_escape_threshold = 8\n"
            "call_depth = 4\n"
            "outside_call_depth = \n"
            "outside_execution_mode = EDGE\n"
            "anonymous_exec_call_depth = \n"
            "anonymous_exec_execution_mode = EDGE\n"
            "module_call_depths = \n"
            "trace_outside_modules = false\n"
            "repeat_hits = false\n"
            "sim_fast_forward = false\n"
            "sim_fast_forward_indirect = false\n"
            "enhanced_sampling = false\n"
            "root_stop_on_return = false\n"
            "\n"
            "[lite]\n"
            "; mode = step   -> use trigger_point + max_events (end_point ignored)\n"
            "; mode = specified -> use trigger_point + end_point (max_events ignored)\n"
            "mode = step\n"
            "; exit_process_on_finish ends the process once the trace stops (step count\n"
            "; reached in step mode, or end_point reached in specified mode).\n"
            "exit_process_on_finish = false\n";
    }
}
