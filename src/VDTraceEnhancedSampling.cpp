#include "pch.h"
#include "VDTraceInternal.h"

namespace vdtrace
{
    namespace
    {
        constexpr uintptr_t kMinSampleAddress = 0x10000;

        bool IsExecutableProtection(DWORD protection)
        {
            const DWORD basic = protection & 0xff;
            return basic == PAGE_EXECUTE
                || basic == PAGE_EXECUTE_READ
                || basic == PAGE_EXECUTE_READWRITE
                || basic == PAGE_EXECUTE_WRITECOPY;
        }

        bool IsReadableProtection(DWORD protection)
        {
            if ((protection & PAGE_GUARD) != 0 || (protection & PAGE_NOACCESS) != 0)
            {
                return false;
            }

            const DWORD basic = protection & 0xff;
            return basic == PAGE_READONLY
                || basic == PAGE_READWRITE
                || basic == PAGE_WRITECOPY
                || basic == PAGE_EXECUTE_READ
                || basic == PAGE_EXECUTE_READWRITE
                || basic == PAGE_EXECUTE_WRITECOPY;
        }

        uintptr_t ResolveSamplingIdentity(const Session::Impl &impl, uintptr_t address)
        {
            if (const auto *range = FindModuleRange(impl.module_ranges, address); range != nullptr)
            {
                return range->base;
            }

            if (const auto *range = FindModuleRange(impl.system_module_ranges, address); range != nullptr)
            {
                return range->base;
            }

            return 0;
        }

        bool IsCrossModuleTransition(const Session::Impl &impl, uintptr_t source, uintptr_t target)
        {
            if (source == 0 || target == 0)
            {
                return false;
            }

            const uintptr_t source_identity = ResolveSamplingIdentity(impl, source);
            const uintptr_t target_identity = ResolveSamplingIdentity(impl, target);
            if (source_identity != 0 && target_identity != 0)
            {
                return source_identity != target_identity;
            }

            return source_identity != target_identity;
        }

        uint8_t ResolveSampleSizeHint(const StepEvent &event, uint8_t argument_index)
        {
            if (argument_index + 1 < event.call_argument_count)
            {
                const uintptr_t hint = event.call_arguments[argument_index + 1];
                if (hint > 0 && hint <= kEnhancedSampleMaxBytes)
                {
                    return static_cast<uint8_t>(hint);
                }
            }

            return kEnhancedSampleMaxBytes;
        }

        bool TryCaptureMemorySample(uintptr_t address, uint8_t source_index, uint8_t size_hint, MemorySample &sample)
        {
            sample = {};
            if (address < kMinSampleAddress)
            {
                return false;
            }

            MEMORY_BASIC_INFORMATION information = {};
            if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &information, sizeof(information)) == 0)
            {
                return false;
            }

            if (information.State != MEM_COMMIT
                || !IsReadableProtection(information.Protect)
                || IsExecutableProtection(information.Protect))
            {
                return false;
            }

            const uintptr_t region_base = reinterpret_cast<uintptr_t>(information.BaseAddress);
            const size_t region_offset = address >= region_base ? static_cast<size_t>(address - region_base) : 0;
            const size_t available = information.RegionSize > region_offset ? information.RegionSize - region_offset : 0;
            if (available == 0)
            {
                return false;
            }

            sample.valid = true;
            sample.source_index = source_index;
            sample.address = address;
            sample.size = static_cast<uint8_t>(std::min<size_t>(std::min<size_t>(size_hint, kEnhancedSampleMaxBytes), available));
            if (sample.size == 0)
            {
                sample = {};
                return false;
            }

            if (!SafeReadMemoryBytes(address, sample.bytes, sample.size))
            {
                sample = {};
                return false;
            }

            return true;
        }
    }

    void PrepareEnhancedSamplingCallEvent(
        const Session::Impl &impl,
        uintptr_t source,
        uintptr_t target,
        const CONTEXT *context,
        StepEvent &event,
        EnhancedSamplingFrame &frame)
    {
        (void)context;
        frame = {};
        if (!impl.options.enhanced_sampling
            || event.kind != EventKind::Call
            || !event.has_target
            || !IsCrossModuleTransition(impl, source, target))
        {
            return;
        }

        frame.active = true;
        for (uint8_t index = 0; index < event.call_argument_count && index < 8; ++index)
        {
            if (event.memory_sample_count >= kEnhancedSampleSlotCount)
            {
                break;
            }

            const uintptr_t argument_value = event.call_arguments[index];
            bool seen = false;
            for (uint8_t existing = 0; existing < frame.argument_count; ++existing)
            {
                if (frame.arguments[existing].valid && frame.arguments[existing].address == argument_value)
                {
                    seen = true;
                    break;
                }
            }
            if (seen)
            {
                continue;
            }

            MemorySample sample = {};
            const uint8_t size_hint = ResolveSampleSizeHint(event, index);
            if (!TryCaptureMemorySample(argument_value, index, size_hint, sample))
            {
                continue;
            }

            event.memory_samples[event.memory_sample_count++] = sample;
            frame.arguments[frame.argument_count].valid = true;
            frame.arguments[frame.argument_count].source_index = index;
            frame.arguments[frame.argument_count].address = sample.address;
            frame.arguments[frame.argument_count].size = sample.size;
            frame.argument_count++;
        }
    }

    void PrepareEnhancedSamplingReturnEvent(
        const Session::Impl &impl,
        const CONTEXT *context,
        StepEvent &event)
    {
        (void)context;
        if (!impl.options.enhanced_sampling
            || event.kind != EventKind::Return
            || impl.enhanced_sampling_stack.empty())
        {
            return;
        }

        const EnhancedSamplingFrame &frame = impl.enhanced_sampling_stack.back();
        if (!frame.active)
        {
            return;
        }

        for (uint8_t index = 0; index < frame.argument_count && index < kEnhancedSampleSlotCount; ++index)
        {
            const EnhancedSamplingTrackedArgument &tracked = frame.arguments[index];
            if (!tracked.valid)
            {
                continue;
            }

            MemorySample sample = {};
            if (!TryCaptureMemorySample(tracked.address, tracked.source_index, tracked.size, sample))
            {
                continue;
            }

            event.memory_samples[event.memory_sample_count++] = sample;
        }

        if (event.has_return_value)
        {
            MemorySample sample = {};
            if (TryCaptureMemorySample(event.return_value, kEnhancedSampleSourceReturnValue, kEnhancedSampleMaxBytes, sample))
            {
                event.has_return_memory_sample = true;
                event.return_memory_sample = sample;
            }
        }
    }

    void PushEnhancedSamplingFrame(Session::Impl &impl, const EnhancedSamplingFrame &frame)
    {
        impl.enhanced_sampling_stack.push_back(frame);
    }

    void PopEnhancedSamplingFrame(Session::Impl &impl)
    {
        if (!impl.enhanced_sampling_stack.empty())
        {
            impl.enhanced_sampling_stack.pop_back();
        }
    }
}
