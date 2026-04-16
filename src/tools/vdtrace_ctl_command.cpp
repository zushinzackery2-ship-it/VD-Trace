#include "vdtrace_ctl_internal.h"

#include <cstring>
#include <iostream>

namespace vdtrace::tools::cli
{
    void PrintUsage()
    {
        std::wcout
            << L"用法:\n"
            << L"  vdtrace_ctl.exe ping <pid>\n"
            << L"  vdtrace_ctl.exe status <pid>\n"
            << L"  vdtrace_ctl.exe stop <pid>\n"
            << L"  vdtrace_ctl.exe start <pid>\n"
            << L"  vdtrace_ctl.exe modules <pid> [all]\n"
            << L"  vdtrace_ctl.exe dump <pid> <module_name> [output_dir]\n"
            << L"  vdtrace_ctl.exe read <pid> <address> [size]\n"
            << L"  vdtrace_ctl.exe write <pid> <address> <hex-bytes>\n"
            << L"  vdtrace_ctl.exe inject <pid> <agent_dll>\n"
            << L"  vdtrace_ctl.exe configure <pid> <thread_id> <modules_csv|- > <output_path> [max_events] [outside] [backend=<dr|tf>] [all] [depth=<n|all|single>] [depthfilter=<outside=<d>[:edge|tf],anon=<d>[:edge|tf],module=<name>:<d>[:edge|tf]...>] [hits=<first|every>] [idleescape=<n>] [sample] [autothread] [blockmain] [focus=<single|queue>] [trigger=<0xaddr|module!0xrva>] [probe=<rule[;rule...]>] [rootstop] [handoff]\n"
            << L"  vdtrace_ctl.exe quickstart <pid> <agent_dll> <thread_id> <modules_csv|- > <output_path> [max_events] [outside] [backend=<dr|tf>] [all] [depth=<n|all|single>] [depthfilter=<outside=<d>[:edge|tf],anon=<d>[:edge|tf],module=<name>:<d>[:edge|tf]...>] [hits=<first|every>] [idleescape=<n>] [sample] [autothread] [blockmain] [focus=<single|queue>] [trigger=<0xaddr|module!0xrva>] [probe=<rule[;rule...]>] [rootstop] [handoff]\n";
    }

    bool ApplyConfigureOption(const std::wstring &text, vdtrace::IpcCommand &command)
    {
        if (_wcsicmp(text.c_str(), L"outside") == 0)
        {
            command.configure.trace_outside_modules = 1u;
            return true;
        }

        if (_wcsicmp(text.c_str(), L"all") == 0)
        {
            command.configure.backend = static_cast<uint32_t>(vdtrace::TraceBackend::TfFullTrace);
            command.configure.control_flow_only = 0u;
            return true;
        }

        if (text.rfind(L"backend=", 0) == 0)
        {
            const std::wstring mode = text.substr(8);
            if (_wcsicmp(mode.c_str(), L"dr") == 0)
            {
                command.configure.backend = static_cast<uint32_t>(vdtrace::TraceBackend::DrControlFlow);
                command.configure.control_flow_only = 1u;
                return true;
            }

            if (_wcsicmp(mode.c_str(), L"tf") == 0)
            {
                command.configure.backend = static_cast<uint32_t>(vdtrace::TraceBackend::TfFullTrace);
                command.configure.control_flow_only = 0u;
                return true;
            }

            return false;
        }

        if (text.rfind(L"depth=", 0) == 0)
        {
            uint32_t max_call_depth = vdtrace::kUnlimitedCallDepth;
            if (!ParseCallDepthText(text.substr(6), max_call_depth))
            {
                return false;
            }

            command.configure.max_call_depth = max_call_depth;
            return true;
        }

        if (text.rfind(L"hits=", 0) == 0)
        {
            const std::wstring mode = text.substr(5);
            if (_wcsicmp(mode.c_str(), L"every") == 0)
            {
                command.configure.hit_policy = static_cast<uint32_t>(vdtrace::FlowHitPolicy::EveryHit);
                return true;
            }

            if (_wcsicmp(mode.c_str(), L"first") == 0)
            {
                command.configure.hit_policy = static_cast<uint32_t>(vdtrace::FlowHitPolicy::FirstSeen);
                return true;
            }

            return false;
        }

        if (text.rfind(L"idleescape=", 0) == 0)
        {
            uint64_t threshold = 0;
            if (!ParseUint64(text.substr(11).c_str(), threshold) || threshold > 0xFFFFFFFFull)
            {
                return false;
            }

            command.configure.hot_bypass_threshold = static_cast<uint32_t>(threshold);
            return true;
        }

        if (_wcsicmp(text.c_str(), L"rootstop") == 0)
        {
            command.configure.stop_on_root_return = 1u;
            return true;
        }

        if (_wcsicmp(text.c_str(), L"handoff") == 0)
        {
            command.configure.async_thread_handoff = 1u;
            return true;
        }

        if (_wcsicmp(text.c_str(), L"sample") == 0)
        {
            command.configure.enhanced_sampling = 1u;
            return true;
        }

        if (_wcsicmp(text.c_str(), L"autothread") == 0)
        {
            command.configure.auto_select_thread = 1u;
            return true;
        }

        if (_wcsicmp(text.c_str(), L"blockmain") == 0)
        {
            command.configure.block_main_thread = 1u;
            return true;
        }

        if (text.rfind(L"focus=", 0) == 0)
        {
            const std::wstring mode = text.substr(6);
            if (_wcsicmp(mode.c_str(), L"single") == 0)
            {
                command.configure.queue_trigger_threads = 0u;
                return true;
            }

            if (_wcsicmp(mode.c_str(), L"queue") == 0)
            {
                command.configure.queue_trigger_threads = 1u;
                return true;
            }

            return false;
        }

        if (text.rfind(L"trigger=", 0) == 0)
        {
            const std::string trigger_utf8 = NarrowUtf8(text.substr(8));
            if (trigger_utf8.size() >= sizeof(command.configure.trigger_point))
            {
                return false;
            }

            std::memset(command.configure.trigger_point, 0, sizeof(command.configure.trigger_point));
            std::memcpy(command.configure.trigger_point, trigger_utf8.data(), trigger_utf8.size());
            return true;
        }

        if (text.rfind(L"probe=", 0) == 0)
        {
            const std::string probe_utf8 = NarrowUtf8(text.substr(6));
            if (probe_utf8.size() >= sizeof(command.configure.probe_spec))
            {
                return false;
            }

            std::memset(command.configure.probe_spec, 0, sizeof(command.configure.probe_spec));
            std::memcpy(command.configure.probe_spec, probe_utf8.data(), probe_utf8.size());
            return true;
        }

        if (text.rfind(L"depthfilter=", 0) == 0)
        {
            const std::string depth_filter_utf8 = NarrowUtf8(text.substr(12));
            if (depth_filter_utf8.size() >= sizeof(command.configure.depth_filter_spec))
            {
                return false;
            }

            std::memset(command.configure.depth_filter_spec, 0, sizeof(command.configure.depth_filter_spec));
            std::memcpy(command.configure.depth_filter_spec, depth_filter_utf8.data(), depth_filter_utf8.size());
            return true;
        }

        return false;
    }

    void PrintResult(const CommandResult &result)
    {
        std::wcout << (result.success ? L"[ok] " : L"[fail] ") << result.message << L"\n";
    }

    vdtrace::IpcCommand BuildSimpleCommand(vdtrace::IpcCommandType type)
    {
        vdtrace::IpcCommand command = {};
        command.version = vdtrace::kIpcVersion;
        command.type = type;
        return command;
    }

    bool BuildConfigureCommand(int argc, wchar_t **argv, int start_index, vdtrace::IpcCommand &command)
    {
        if (argc - start_index < 3)
        {
            return false;
        }

        DWORD thread_id = 0;
        if (!ParseDword(argv[start_index], thread_id))
        {
            return false;
        }

        const std::wstring modules = argv[start_index + 1];
        const std::wstring output_path = argv[start_index + 2];
        uint64_t max_events = 0;
        if (argc - start_index >= 4)
        {
            ParseUint64(argv[start_index + 3], max_events);
        }

        command = {};
        command.version = vdtrace::kIpcVersion;
        command.type = vdtrace::IpcCommandType::Configure;
        command.configure.thread_id = static_cast<int32_t>(thread_id);
        command.configure.max_events = 0;
        command.configure.trace_outside_modules = 0u;
        command.configure.backend = static_cast<uint32_t>(vdtrace::TraceBackend::DrControlFlow);
        command.configure.control_flow_only = 1u;
        command.configure.max_call_depth = vdtrace::kUnlimitedCallDepth;
        command.configure.hit_policy = static_cast<uint32_t>(vdtrace::FlowHitPolicy::FirstSeen);
        command.configure.hot_bypass_threshold = 32u;
        command.configure.enhanced_sampling = 0u;
        command.configure.stop_on_root_return = 0u;
        command.configure.async_thread_handoff = 0u;
        command.configure.block_main_thread = 0u;
        command.configure.queue_trigger_threads = 0u;

        int option_index = start_index + 3;
        if (argc > option_index && ParseUint64(argv[option_index], max_events))
        {
            command.configure.max_events = max_events;
            option_index++;
        }

        for (int index = option_index; index < argc; ++index)
        {
            if (!ApplyConfigureOption(argv[index], command))
            {
                return false;
            }
        }

        const std::wstring normalized_modules = modules == L"-" ? L"" : modules;
        const std::string modules_utf8 = NarrowUtf8(normalized_modules);
        const std::string output_utf8 = NarrowUtf8(output_path);
        if (modules_utf8.size() >= sizeof(command.configure.module_names) || output_utf8.size() >= sizeof(command.configure.output_path))
        {
            return false;
        }

        std::memcpy(command.configure.module_names, modules_utf8.data(), modules_utf8.size());
        std::memcpy(command.configure.output_path, output_utf8.data(), output_utf8.size());
        return true;
    }
}
