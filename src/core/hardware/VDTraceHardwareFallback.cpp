#include "pch.h"
#include "core/runtime/VDTraceInternal.h"

namespace vdtrace
{
    bool BeginHardwareLinearScan(Session::Impl &impl, uintptr_t entry, CONTEXT *context, std::wstring &error)
    {
        impl.observation_state = ObservationState::LinearScan;
        impl.last_rip = entry;
        impl.last_rip_valid = true;

        uintptr_t empty[4] = {};
        if (context != nullptr)
        {
            ApplyHardwareContextObservations(*context, empty, 0, true);
            return true;
        }

        return ArmSingleStep(impl.thread_handle, error);
    }
}
