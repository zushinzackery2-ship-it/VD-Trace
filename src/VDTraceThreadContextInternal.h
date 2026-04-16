#ifndef VDTRACE_THREAD_CONTEXT_INTERNAL_H
#define VDTRACE_THREAD_CONTEXT_INTERNAL_H

#include "VDTraceInternal.h"

namespace vdtrace::thread_context_detail
{
    std::wstring GetModuleFilePath(HMODULE module);
    std::wstring ToLowerCopy(const std::wstring &text);
    bool StartsWith(const std::wstring &text, const std::wstring &prefix);
    bool IsKnownSystemModuleName(const std::wstring &name);
    bool IsSystemModulePath(const std::wstring &path);
    bool TryReadPointerValue(uintptr_t pointer_value, uintptr_t &value);

    template <typename TUpdate>
    bool UpdateThreadContext(HANDLE thread_handle, DWORD context_flags, TUpdate update, std::wstring &error)
    {
        if (thread_handle == nullptr)
        {
            error = L"线程句柄无效。";
            return false;
        }

        const DWORD suspend_result = SuspendThread(thread_handle);
        if (suspend_result == static_cast<DWORD>(-1))
        {
            error = FormatWin32Error(L"挂起目标线程失败。", GetLastError());
            return false;
        }

        CONTEXT context = {};
        context.ContextFlags = context_flags;
        bool success = false;
        if (!GetThreadContext(thread_handle, &context))
        {
            error = FormatWin32Error(L"读取目标线程上下文失败。", GetLastError());
        }
        else if (!update(context))
        {
            success = false;
        }
        else if (!SetThreadContext(thread_handle, &context))
        {
            error = FormatWin32Error(L"写回目标线程上下文失败。", GetLastError());
        }
        else
        {
            success = true;
        }

        ResumeThread(thread_handle);
        return success;
    }

    void ApplyExecutionControls(CONTEXT &context, const uintptr_t *addresses, uint32_t count, bool single_step);
}

#endif
