#include "pch.h"
#include "control/ipc_client/VDTraceControlSupport.h"
#include "control/ipc_client/VDTraceControlSupportInternal.h"

namespace vdtrace::tools::detail
{
    CommandResult MakeResult(bool success, int32_t status, const std::wstring &message)
    {
        CommandResult result;
        result.success = success;
        result.status = status;
        result.message = message;
        return result;
    }

    CommandResult MakeWin32ErrorResult(const std::wstring &prefix, DWORD error)
    {
        wchar_t buffer[256] = {};
        FormatMessageW(
            FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error,
            0,
            buffer,
            static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0])),
            nullptr);

        std::wstring text = prefix + L" (error=" + std::to_wstring(error) + L")";
        if (buffer[0] != L'\0')
        {
            text += L" ";
            text += buffer;
        }

        return MakeResult(false, IPC_STATUS_INTERNAL_ERROR, text);
    }
}

namespace vdtrace::tools
{
    CommandResult Configure(
        DWORD pid,
        DWORD thread_id,
        const std::wstring &modules,
        const std::wstring &output_path,
        uint64_t max_events,
        bool trace_outside_modules,
        TraceBackend backend,
        bool control_flow_only,
        uint32_t max_call_depth,
        const std::wstring &depth_filter_spec,
        FlowHitPolicy hit_policy,
        uint32_t hot_bypass_threshold,
        bool enhanced_sampling,
        bool auto_select_thread,
        bool block_main_thread,
        bool queue_trigger_threads,
        const std::wstring &trigger_point,
        const std::wstring &probe_spec,
        bool stop_on_root_return,
        bool async_thread_handoff,
        DWORD timeout_ms)
    {
        IpcCommand command = {};
        command.version = kIpcVersion;
        command.type = IpcCommandType::Configure;
        command.configure.thread_id = static_cast<int32_t>(thread_id);
        command.configure.max_events = max_events;
        command.configure.trace_outside_modules = trace_outside_modules ? 1u : 0u;
        command.configure.backend = static_cast<uint32_t>(backend);
        command.configure.control_flow_only = control_flow_only ? 1u : 0u;
        command.configure.max_call_depth = max_call_depth;
        command.configure.hit_policy = static_cast<uint32_t>(hit_policy);
        command.configure.hot_bypass_threshold = hot_bypass_threshold;
        command.configure.enhanced_sampling = enhanced_sampling ? 1u : 0u;
        command.configure.auto_select_thread = auto_select_thread ? 1u : 0u;
        command.configure.block_main_thread = block_main_thread ? 1u : 0u;
        command.configure.queue_trigger_threads = queue_trigger_threads ? 1u : 0u;
        command.configure.stop_on_root_return = stop_on_root_return ? 1u : 0u;
        command.configure.async_thread_handoff = async_thread_handoff ? 1u : 0u;

        const std::wstring normalized_modules = modules == L"-" ? L"" : modules;
        const std::string modules_utf8 = detail::NarrowUtf8(normalized_modules);
        const std::string output_utf8 = detail::NarrowUtf8(output_path);
        const std::string trigger_utf8 = detail::NarrowUtf8(trigger_point);
        const std::string probe_utf8 = detail::NarrowUtf8(probe_spec);
        const std::string depth_filter_utf8 = detail::NarrowUtf8(depth_filter_spec);
        if (modules_utf8.size() >= sizeof(command.configure.module_names)
            || output_utf8.size() >= sizeof(command.configure.output_path)
            || trigger_utf8.size() >= sizeof(command.configure.trigger_point)
            || probe_utf8.size() >= sizeof(command.configure.probe_spec)
            || depth_filter_utf8.size() >= sizeof(command.configure.depth_filter_spec))
        {
            return detail::MakeResult(false, IPC_STATUS_INVALID_ARGUMENT, L"模块名、输出路径、触发点、观测器规则或层级过滤规则过长。");
        }

        std::memcpy(command.configure.module_names, modules_utf8.data(), modules_utf8.size());
        std::memcpy(command.configure.output_path, output_utf8.data(), output_utf8.size());
        std::memcpy(command.configure.trigger_point, trigger_utf8.data(), trigger_utf8.size());
        std::memcpy(command.configure.probe_spec, probe_utf8.data(), probe_utf8.size());
        std::memcpy(command.configure.depth_filter_spec, depth_filter_utf8.data(), depth_filter_utf8.size());
        return SendCommand(pid, command, timeout_ms);
    }

    CommandResult Start(DWORD pid, DWORD timeout_ms)
    {
        IpcCommand command = {};
        command.version = kIpcVersion;
        command.type = IpcCommandType::Start;
        return SendCommand(pid, command, timeout_ms);
    }

    CommandResult ListModules(DWORD pid, bool include_system_modules, DWORD timeout_ms)
    {
        IpcCommand command = {};
        command.version = kIpcVersion;
        command.type = IpcCommandType::ListModules;
        command.list_modules.include_system_modules = include_system_modules ? 1u : 0u;
        return SendCommand(pid, command, timeout_ms);
    }

    CommandResult DumpModule(DWORD pid, const std::wstring &module_name, const std::wstring &output_directory, DWORD timeout_ms)
    {
        const std::wstring normalized_output_directory = output_directory.empty()
            ? BuildDefaultDumpOutputDirectory()
            : output_directory;
        const std::string module_utf8 = detail::NarrowUtf8(module_name);
        const std::string output_utf8 = detail::NarrowUtf8(normalized_output_directory);
        IpcCommand command = {};
        command.version = kIpcVersion;
        command.type = IpcCommandType::DumpModule;
        if (module_utf8.empty() || module_utf8.size() >= sizeof(command.dump_module.module_name))
        {
            return detail::MakeResult(false, IPC_STATUS_INVALID_ARGUMENT, L"模块名为空或过长。");
        }

        if (output_utf8.empty() || output_utf8.size() >= sizeof(command.dump_module.output_directory))
        {
            return detail::MakeResult(false, IPC_STATUS_INVALID_ARGUMENT, L"Dump 输出目录为空或过长。");
        }

        std::memcpy(command.dump_module.module_name, module_utf8.data(), module_utf8.size());
        std::memcpy(command.dump_module.output_directory, output_utf8.data(), output_utf8.size());
        return SendCommand(pid, command, timeout_ms);
    }

    CommandResult ReadMemory(DWORD pid, const std::wstring &address_text, uint32_t size, DWORD timeout_ms)
    {
        const std::string address_utf8 = detail::NarrowUtf8(address_text);
        IpcCommand command = {};
        command.version = kIpcVersion;
        command.type = IpcCommandType::ReadMemory;
        if (address_utf8.empty() || address_utf8.size() >= sizeof(command.read_memory.address_text))
        {
            return detail::MakeResult(false, IPC_STATUS_INVALID_ARGUMENT, L"内存地址为空或过长。");
        }

        if (size == 0 || size > kIpcMemoryWriteCapacity)
        {
            return detail::MakeResult(false, IPC_STATUS_INVALID_ARGUMENT, L"读取长度必须在 1 到 512 之间。");
        }

        command.read_memory.size = size;
        std::memcpy(command.read_memory.address_text, address_utf8.data(), address_utf8.size());
        return SendCommand(pid, command, timeout_ms);
    }

    CommandResult WriteMemory(DWORD pid, const std::wstring &address_text, const uint8_t *bytes, uint32_t size, DWORD timeout_ms)
    {
        const std::string address_utf8 = detail::NarrowUtf8(address_text);
        IpcCommand command = {};
        command.version = kIpcVersion;
        command.type = IpcCommandType::WriteMemory;
        if (address_utf8.empty() || address_utf8.size() >= sizeof(command.write_memory.address_text))
        {
            return detail::MakeResult(false, IPC_STATUS_INVALID_ARGUMENT, L"内存地址为空或过长。");
        }

        if (bytes == nullptr || size == 0 || size > kIpcMemoryWriteCapacity)
        {
            return detail::MakeResult(false, IPC_STATUS_INVALID_ARGUMENT, L"写入长度必须在 1 到 512 之间。");
        }

        command.write_memory.size = size;
        std::memcpy(command.write_memory.address_text, address_utf8.data(), address_utf8.size());
        std::memcpy(command.write_memory.bytes, bytes, size);
        return SendCommand(pid, command, timeout_ms);
    }

    CommandResult Stop(DWORD pid, DWORD timeout_ms)
    {
        IpcCommand command = {};
        command.version = kIpcVersion;
        command.type = IpcCommandType::Stop;
        return SendCommand(pid, command, timeout_ms);
    }

    CommandResult Status(DWORD pid, DWORD timeout_ms)
    {
        IpcCommand command = {};
        command.version = kIpcVersion;
        command.type = IpcCommandType::Status;
        return SendCommand(pid, command, timeout_ms);
    }

    CommandResult SendCommand(DWORD pid, const IpcCommand &command, DWORD timeout_ms)
    {
        const std::wstring pipe_name = BuildPipeName(pid);
        const bool waited = WaitForPipeReady(pid, timeout_ms);
        const DWORD wait_error = GetLastError();

        HANDLE pipe = CreateFileW(
            pipe_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            return detail::MakeWin32ErrorResult(waited ? L"打开 VD-Trace IPC 失败。" : L"连接 VD-Trace IPC 失败。", waited ? GetLastError() : wait_error);
        }

        DWORD read_mode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(pipe, &read_mode, nullptr, nullptr);

        DWORD bytes_written = 0;
        if (!WriteFile(pipe, &command, sizeof(command), &bytes_written, nullptr) || bytes_written != sizeof(command))
        {
            const DWORD error = GetLastError();
            CloseHandle(pipe);
            return detail::MakeWin32ErrorResult(L"发送 VD-Trace 命令失败。", error);
        }

        IpcResponse response = {};
        DWORD bytes_read = 0;
        if (!ReadFile(pipe, &response, sizeof(response), &bytes_read, nullptr) || bytes_read != sizeof(response))
        {
            const DWORD error = GetLastError();
            CloseHandle(pipe);
            return detail::MakeWin32ErrorResult(L"读取 VD-Trace 响应失败。", error);
        }

        CloseHandle(pipe);

        int count = MultiByteToWideChar(CP_UTF8, 0, response.message, -1, nullptr, 0);
        std::wstring message;
        if (count > 1)
        {
            message.resize(static_cast<size_t>(count));
            MultiByteToWideChar(CP_UTF8, 0, response.message, -1, message.data(), count);
            if (!message.empty() && message.back() == L'\0')
            {
                message.pop_back();
            }
        }

        return detail::MakeResult(response.status == IPC_STATUS_OK, response.status, message);
    }
}
