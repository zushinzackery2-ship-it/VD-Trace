#ifndef VDTRACE_LITE_RUNTIME_H
#define VDTRACE_LITE_RUNTIME_H

#include "lite/LiteTraceConfig.h"

namespace vdtrace::lite
{
    DWORD RunLiteTrace();
    DWORD WINAPI LiteTraceThreadMain(void *parameter);
}

#endif
