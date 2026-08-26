#include "pch.h"
#include "core/runtime/VDTraceInternal.h"

namespace vdtrace
{
    bool ResolveTriggerAddress(const Options &options, uintptr_t &resolved_address, std::wstring &error)
    {
        resolved_address = options.trigger_address;
        if (options.trigger_module_name.empty())
        {
            return true;
        }

        ModuleRange trigger_range = {};
        if (!ResolveModuleRange(options.trigger_module_name, trigger_range, error))
        {
            return false;
        }

        resolved_address = trigger_range.base + options.trigger_address;
        return true;
    }

    bool ResolveStopAddress(const Options &options, uintptr_t &resolved_address, std::wstring &error)
    {
        resolved_address = options.stop_address;
        if (options.stop_module_name.empty())
        {
            return true;
        }

        ModuleRange stop_range = {};
        if (!ResolveModuleRange(options.stop_module_name, stop_range, error))
        {
            return false;
        }

        resolved_address = stop_range.base + options.stop_address;
        return true;
    }

    size_t DetermineSeenEdgeReserve(const Options &options)
    {
        constexpr size_t kDefaultReserve = 65536;
        constexpr size_t kMaxReserve = 1u << 20;
        if (options.hit_policy != FlowHitPolicy::FirstSeen)
        {
            return kDefaultReserve;
        }

        if (options.max_events == 0)
        {
            return kDefaultReserve;
        }

        const uint64_t capped = std::min<uint64_t>(options.max_events, static_cast<uint64_t>(kMaxReserve));
        return static_cast<size_t>(std::max<uint64_t>(capped, 1024));
    }
}
