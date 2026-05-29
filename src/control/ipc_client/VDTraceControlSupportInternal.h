#ifndef VDTRACE_CONTROL_SUPPORT_INTERNAL_H
#define VDTRACE_CONTROL_SUPPORT_INTERNAL_H

#include "control/ipc_client/VDTraceControlSupport.h"

namespace vdtrace::tools::detail
{
    CommandResult MakeResult(bool success, int32_t status, const std::wstring &message);
    CommandResult MakeWin32ErrorResult(const std::wstring &prefix, DWORD error);
    std::wstring BuildTimestampSuffix();
    std::string NarrowUtf8(const std::wstring &text);
    bool GetThreadCreationTime(HANDLE thread_handle, FILETIME &creation_time);
}

#endif
