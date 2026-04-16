#include "pch.h"
#include "VDTraceInternal.h"

namespace vdtrace
{
    namespace
    {
        struct ThreadRegionQueryCache
        {
            const Session::Impl *impl = nullptr;
            uintptr_t base = 0;
            uintptr_t end = 0;
            bool valid = false;
            bool executable = false;
            bool is_image = false;
        };

        ThreadRegionQueryCache &RegionQueryCacheSlot()
        {
            thread_local ThreadRegionQueryCache cache = {};
            return cache;
        }

        bool IsExecutableProtection(DWORD protection)
        {
            const DWORD basic = protection & 0xff;
            return basic == PAGE_EXECUTE
                || basic == PAGE_EXECUTE_READ
                || basic == PAGE_EXECUTE_READWRITE
                || basic == PAGE_EXECUTE_WRITECOPY;
        }

        bool TryReadCachedRegion(
            const Session::Impl &impl,
            uintptr_t address,
            bool &executable,
            bool &is_image,
            size_t *region_size)
        {
            ThreadRegionQueryCache &cache = RegionQueryCacheSlot();
            if (!cache.valid
                || cache.impl != &impl
                || address < cache.base
                || address >= cache.end)
            {
                return false;
            }

            executable = cache.executable;
            is_image = cache.is_image;
            if (region_size != nullptr)
            {
                *region_size = cache.end - cache.base;
            }
            return true;
        }

        void StoreCachedRegion(
            const Session::Impl &impl,
            const MEMORY_BASIC_INFORMATION &mbi,
            bool executable,
            bool is_image)
        {
            ThreadRegionQueryCache &cache = RegionQueryCacheSlot();
            cache.impl = &impl;
            cache.base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
            cache.end = cache.base + mbi.RegionSize;
            cache.valid = true;
            cache.executable = executable;
            cache.is_image = is_image;
        }
    }

    const ResolvedDepthFilterModuleRule *FindDepthFilterModuleRule(const ResolvedDepthFilterSet &filters, uintptr_t address)
    {
        if (filters.module_rules.empty() || address == 0)
        {
            return nullptr;
        }

        const auto it = std::upper_bound(
            filters.module_rules.begin(),
            filters.module_rules.end(),
            address,
            [](uintptr_t value, const ResolvedDepthFilterModuleRule &rule)
            {
                return value < rule.range.base;
            });
        if (it == filters.module_rules.begin())
        {
            return nullptr;
        }

        const auto &rule = *std::prev(it);
        const uintptr_t end = rule.range.base + rule.range.size;
        return address >= rule.range.base && address < end ? &rule : nullptr;
    }

    ResolvedExecutionAddress ResolveExecutionAddress(const Session::Impl &impl, uintptr_t address, size_t *region_size)
    {
        ResolvedExecutionAddress resolved = {};
        if (region_size != nullptr)
        {
            *region_size = 0;
        }

        if (address == 0)
        {
            return resolved;
        }

        if (const auto *module = FindModuleRange(impl.system_module_ranges, address); module != nullptr)
        {
            resolved.kind = ExecutionAddressKind::SystemModule;
            resolved.module_range = module;
            return resolved;
        }

        if (const auto *rule = FindDepthFilterModuleRule(impl.depth_filters, address); rule != nullptr)
        {
            resolved.kind = ExecutionAddressKind::TrackedModule;
            resolved.module_range = &rule->range;
            resolved.depth_rule = rule;
            return resolved;
        }

        if (const auto *module = FindModuleRange(impl.module_ranges, address); module != nullptr)
        {
            resolved.kind = ExecutionAddressKind::TrackedModule;
            resolved.module_range = module;
            return resolved;
        }

        bool is_image = false;
        if (!QueryNonModuleExecutableRegion(impl, address, is_image, &resolved.region_size))
        {
            return resolved;
        }

        resolved.kind = is_image
            ? ExecutionAddressKind::OutsideImage
            : ExecutionAddressKind::AnonymousExecutable;
        if (region_size != nullptr)
        {
            *region_size = resolved.region_size;
        }
        return resolved;
    }

    bool QueryNonModuleExecutableRegion(const Session::Impl &impl, uintptr_t address, bool &is_image, size_t *region_size)
    {
        is_image = false;
        if (region_size != nullptr)
        {
            *region_size = 0;
        }

        if (address == 0
            || FindModuleRange(impl.module_ranges, address) != nullptr
            || FindModuleRange(impl.system_module_ranges, address) != nullptr)
        {
            return false;
        }

        bool executable = false;
        if (TryReadCachedRegion(impl, address, executable, is_image, region_size))
        {
            return executable;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &mbi, sizeof(mbi)) == 0)
        {
            return false;
        }

        executable = mbi.State == MEM_COMMIT && IsExecutableProtection(mbi.Protect);
        is_image = executable && mbi.Type == MEM_IMAGE;
        if (executable)
        {
            StoreCachedRegion(impl, mbi, executable, is_image);
        }
        if (region_size != nullptr)
        {
            *region_size = mbi.RegionSize;
        }
        return executable;
    }

    uint32_t ResolveCallDepthLimitForAddress(const Session::Impl &impl, uintptr_t address, bool &has_limit, bool &is_system_target)
    {
        has_limit = false;
        is_system_target = false;

        const ResolvedExecutionAddress resolved = ResolveExecutionAddress(impl, address);
        if (resolved.kind == ExecutionAddressKind::SystemModule)
        {
            is_system_target = true;
            return 0;
        }

        if (resolved.depth_rule != nullptr)
        {
            has_limit = true;
            return resolved.depth_rule->max_call_depth;
        }

        if (resolved.kind == ExecutionAddressKind::TrackedModule)
        {
            has_limit = true;
            return impl.options.max_call_depth;
        }

        if (resolved.kind == ExecutionAddressKind::OutsideImage)
        {
            has_limit = true;
            return impl.depth_filters.has_outside_module_depth
                ? impl.depth_filters.outside_module_depth
                : impl.options.max_call_depth;
        }

        if (resolved.kind == ExecutionAddressKind::AnonymousExecutable)
        {
            has_limit = true;
            return impl.depth_filters.has_anonymous_exec_depth
                ? impl.depth_filters.anonymous_exec_depth
                : impl.options.max_call_depth;
        }

        return 0;
    }

    DepthFilterExecutionMode ResolveExecutionModeForAddress(
        const Session::Impl &impl,
        uintptr_t address,
        bool &has_mode,
        bool &is_system_target)
    {
        has_mode = false;
        is_system_target = false;

        const ResolvedExecutionAddress resolved = ResolveExecutionAddress(impl, address);
        if (resolved.kind == ExecutionAddressKind::SystemModule)
        {
            is_system_target = true;
            return DepthFilterExecutionMode::Edge;
        }

        if (resolved.depth_rule != nullptr)
        {
            has_mode = resolved.depth_rule->execution_mode == DepthFilterExecutionMode::TrapFlag;
            return resolved.depth_rule->execution_mode;
        }

        if (resolved.kind == ExecutionAddressKind::TrackedModule)
        {
            return DepthFilterExecutionMode::Edge;
        }

        if (resolved.kind == ExecutionAddressKind::OutsideImage)
        {
            has_mode = impl.depth_filters.has_outside_module_depth
                && impl.depth_filters.outside_module_execution_mode == DepthFilterExecutionMode::TrapFlag;
            return impl.depth_filters.has_outside_module_depth
                ? impl.depth_filters.outside_module_execution_mode
                : DepthFilterExecutionMode::Edge;
        }

        if (resolved.kind == ExecutionAddressKind::AnonymousExecutable)
        {
            has_mode = impl.depth_filters.has_anonymous_exec_depth
                && impl.depth_filters.anonymous_exec_execution_mode == DepthFilterExecutionMode::TrapFlag;
            return impl.depth_filters.has_anonymous_exec_depth
                ? impl.depth_filters.anonymous_exec_execution_mode
                : DepthFilterExecutionMode::Edge;
        }

        return DepthFilterExecutionMode::Edge;
    }
}
