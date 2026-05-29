#include "pch.h"
#include "VDTrace/VDTrace.h"

namespace vdtrace
{
    const wchar_t *EventKindName(EventKind kind)
    {
        switch (kind)
        {
        case EventKind::Other:
            return L"other";
        case EventKind::Probe:
            return L"probe";
        case EventKind::Call:
            return L"call";
        case EventKind::Jump:
            return L"jump";
        case EventKind::ConditionalJump:
            return L"jcc";
        case EventKind::Return:
            return L"ret";
        case EventKind::Syscall:
            return L"syscall";
        case EventKind::Interrupt:
            return L"int";
        case EventKind::Unknown:
        default:
            return L"unknown";
        }
    }

    const wchar_t *FlowHitPolicyName(FlowHitPolicy policy)
    {
        switch (policy)
        {
        case FlowHitPolicy::EveryHit:
            return L"every";
        case FlowHitPolicy::FirstSeen:
        default:
            return L"first";
        }
    }

    const wchar_t *TraceBackendName(TraceBackend backend)
    {
        switch (backend)
        {
        case TraceBackend::TfFullTrace:
            return L"tf";
        case TraceBackend::PtControlFlow:
            return L"pt";
        case TraceBackend::DrControlFlow:
        default:
            return L"dr";
        }
    }
}
