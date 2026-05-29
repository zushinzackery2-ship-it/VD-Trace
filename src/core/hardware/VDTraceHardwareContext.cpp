#include "pch.h"
#include "core/runtime/VDTraceInternal.h"

namespace vdtrace
{
    void ApplyHardwareContextObservations(CONTEXT &context, const uintptr_t *addresses, uint32_t count, bool single_step)
    {
        context.ContextFlags |= CONTEXT_CONTROL | CONTEXT_DEBUG_REGISTERS;
        context.Dr0 = count > 0 ? addresses[0] : 0;
        context.Dr1 = count > 1 ? addresses[1] : 0;
        context.Dr2 = count > 2 ? addresses[2] : 0;
        context.Dr3 = count > 3 ? addresses[3] : 0;
        context.Dr6 = 0;
        context.Dr7 = 0;
        for (uint32_t index = 0; index < count && index < 4; index++)
        {
            context.Dr7 |= (static_cast<DWORD_PTR>(1) << (index * 2));
        }

        if (single_step)
        {
            context.EFlags |= 0x100u;
        }
        else
        {
            context.EFlags &= ~0x100u;
        }
    }
}
