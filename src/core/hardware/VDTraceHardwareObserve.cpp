#include "pch.h"
#include "core/runtime/VDTraceInternal.h"

namespace vdtrace
{
    uint32_t StoreObservedAddressesUnique(Session::Impl &impl, const uintptr_t *addresses, uint32_t count)
    {
        std::memset(impl.observed_addresses, 0, sizeof(impl.observed_addresses));
        impl.observed_count = 0;
        for (uint32_t index = 0; index < count && index < 4; index++)
        {
            if (addresses[index] == 0)
            {
                continue;
            }

            bool duplicate = false;
            for (uint32_t existing = 0; existing < impl.observed_count; existing++)
            {
                if (impl.observed_addresses[existing] == addresses[index])
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
            {
                impl.observed_addresses[impl.observed_count++] = addresses[index];
            }
        }

        return impl.observed_count;
    }
}
