#include "pch.h"
#include "tools/VDTraceControlSupport.h"
#include "tools/VDTraceControlSupportInternal.h"

namespace vdtrace::tools
{
    namespace
    {
        CommandResult InjectBootstrapDllInternal(DWORD pid, const std::wstring &dll_path, const wchar_t *path_error_prefix, const char *bootstrap_name, const wchar_t *bootstrap_error_text, bool wait_for_ipc_ready, DWORD timeout_ms)
        {
            std::wstring full_path;
            if (!ResolveFullPath(dll_path, full_path))
            {
                return detail::MakeResult(false, IPC_STATUS_INVALID_ARGUMENT, path_error_prefix);
            }

            HANDLE process = OpenProcess(
                PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                FALSE,
                pid);
            if (process == nullptr)
            {
                return detail::MakeWin32ErrorResult(L"打开目标进程失败。", GetLastError());
            }

            const SIZE_T bytes = (full_path.size() + 1) * sizeof(wchar_t);
            LPVOID remote_path = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (remote_path == nullptr)
            {
                const DWORD error = GetLastError();
                CloseHandle(process);
                return detail::MakeWin32ErrorResult(L"写入 DLL 路径前分配远程内存失败。", error);
            }

            if (!WriteProcessMemory(process, remote_path, full_path.c_str(), bytes, nullptr))
            {
                const DWORD error = GetLastError();
                VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
                CloseHandle(process);
                return detail::MakeWin32ErrorResult(L"写入 DLL 路径失败。", error);
            }

            HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
            auto load_library = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel32, "LoadLibraryW"));
            if (load_library == nullptr)
            {
                VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
                CloseHandle(process);
                return detail::MakeResult(false, IPC_STATUS_INTERNAL_ERROR, L"获取 LoadLibraryW 地址失败。");
            }

            HANDLE load_thread = CreateRemoteThread(process, nullptr, 0, load_library, remote_path, 0, nullptr);
            if (load_thread == nullptr)
            {
                const DWORD error = GetLastError();
                VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
                CloseHandle(process);
                return detail::MakeWin32ErrorResult(L"创建远程 LoadLibrary 线程失败。", error);
            }

            WaitForSingleObject(load_thread, INFINITE);

            DWORD remote_module_base = 0;
            GetExitCodeThread(load_thread, &remote_module_base);
            CloseHandle(load_thread);
            VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
            if (remote_module_base == 0)
            {
                CloseHandle(process);
                return detail::MakeResult(false, IPC_STATUS_INTERNAL_ERROR, L"远程 LoadLibraryW 返回 0。");
            }

            HMODULE local_module = LoadLibraryExW(full_path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
            if (local_module == nullptr)
            {
                CloseHandle(process);
                return detail::MakeWin32ErrorResult(L"本地加载目标 DLL 失败。", GetLastError());
            }

            FARPROC bootstrap = GetProcAddress(local_module, bootstrap_name);
            if (bootstrap == nullptr)
            {
                FreeLibrary(local_module);
                CloseHandle(process);
                return detail::MakeResult(false, IPC_STATUS_INTERNAL_ERROR, bootstrap_error_text);
            }

            const uintptr_t rva = reinterpret_cast<uintptr_t>(bootstrap) - reinterpret_cast<uintptr_t>(local_module);
            FreeLibrary(local_module);

            auto remote_bootstrap = reinterpret_cast<LPTHREAD_START_ROUTINE>(static_cast<uintptr_t>(remote_module_base) + rva);
            HANDLE bootstrap_thread = CreateRemoteThread(process, nullptr, 0, remote_bootstrap, nullptr, 0, nullptr);
            if (bootstrap_thread == nullptr)
            {
                const DWORD error = GetLastError();
                CloseHandle(process);
                return detail::MakeWin32ErrorResult(L"创建远程 bootstrap 线程失败。", error);
            }

            WaitForSingleObject(bootstrap_thread, INFINITE);
            DWORD bootstrap_result = 0;
            GetExitCodeThread(bootstrap_thread, &bootstrap_result);
            CloseHandle(bootstrap_thread);
            CloseHandle(process);

            if (bootstrap_result == 0)
            {
                return detail::MakeResult(false, IPC_STATUS_INTERNAL_ERROR, L"远程 bootstrap 返回 0。");
            }

            if (wait_for_ipc_ready)
            {
                if (!WaitForPipeReady(pid, timeout_ms))
                {
                    return detail::MakeResult(false, IPC_STATUS_PIPE_ERROR, L"Agent 已注入，但 IPC 管道未就绪。");
                }

                return detail::MakeResult(true, IPC_STATUS_OK, L"注入并拉起 IPC 成功。");
            }

            return detail::MakeResult(true, IPC_STATUS_OK, L"目标 DLL 已注入并拉起 bootstrap。");
        }
    }

    CommandResult InjectAgent(DWORD pid, const std::wstring &dll_path, DWORD timeout_ms)
    {
        return InjectBootstrapDllInternal(
            pid,
            dll_path,
            L"解析 Agent DLL 路径失败。",
            "vdtrace_agent_bootstrap_ipc",
            L"Agent DLL 缺少 bootstrap 导出。",
            true,
            timeout_ms);
    }

    CommandResult InjectBootstrapDll(DWORD pid, const std::wstring &dll_path)
    {
        return InjectBootstrapDllInternal(
            pid,
            dll_path,
            L"解析 bootstrap DLL 路径失败。",
            "vdtrace_loader_bootstrap",
            L"目标 DLL 缺少 vdtrace_loader_bootstrap 导出。",
            false,
            0);
    }
}
