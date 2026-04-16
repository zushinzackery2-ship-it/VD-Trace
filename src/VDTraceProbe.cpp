#include "pch.h"
#include "VDTraceProbeInternal.h"

namespace vdtrace
{
    bool ParseProbeSpec(const std::wstring &text, std::vector<ResolvedValueProbe> &probes, std::wstring &error)
    {
        using namespace probe_detail;

        probes.clear();
        error.clear();

        const std::vector<std::wstring> rules = SplitText(text, L';');
        for (const std::wstring &rule : rules)
        {
            const std::wstring trimmed = TrimText(rule);
            if (trimmed.empty())
            {
                continue;
            }

            const std::wstring lowered = ToLowerCopy(trimmed);
            if (lowered.rfind(L"step@", 0) == 0 || lowered.rfind(L"write@", 0) == 0)
            {
                ResolvedValueProbe probe = {};
                if (!ParseObserverRule(trimmed, probe, error))
                {
                    return false;
                }

                auto existing = std::find_if(
                    probes.begin(),
                    probes.end(),
                    [&probe](const ResolvedValueProbe &current)
                    {
                        return current.address == probe.address;
                    });
                if (existing != probes.end())
                {
                    error = L"同一个命中点不能重复配置多条观测器规则。";
                    return false;
                }

                probes.push_back(probe);
                continue;
            }

            if (!ParseLegacyCaptureRule(trimmed, probes, error))
            {
                return false;
            }
        }

        std::sort(
            probes.begin(),
            probes.end(),
            [](const ResolvedValueProbe &left, const ResolvedValueProbe &right)
            {
                return left.address < right.address;
            });
        return true;
    }

    bool HasValueProbeInRange(const Session::Impl &impl, uintptr_t begin, uintptr_t end)
    {
        if (impl.value_probes.empty() || begin >= end)
        {
            return false;
        }

        const auto it = std::lower_bound(
            impl.value_probes.begin(),
            impl.value_probes.end(),
            begin,
            [](const ResolvedValueProbe &probe, uintptr_t value)
            {
                return probe.address < value;
            });
        return it != impl.value_probes.end() && it->address < end;
    }

    bool TryEmitValueProbeEvent(Session::Impl &impl, uintptr_t instruction, const CONTEXT *context)
    {
        using namespace probe_detail;

        if (context == nullptr || impl.value_probes.empty())
        {
            return false;
        }

        const ResolvedValueProbe *probe = FindValueProbeByAddress(impl.value_probes, instruction);
        if (probe == nullptr || probe->mode != ProbeMode::Capture)
        {
            return false;
        }

        StepEvent event = {};
        event.sequence = impl.event_count.fetch_add(1) + 1;
        event.thread_id = GetCurrentThreadId();
        event.instruction = instruction;
        event.stack_pointer = static_cast<uintptr_t>(context->Rsp);
        event.block_begin = instruction;
        event.call_depth = impl.current_call_depth.load();
        event.kind = EventKind::Probe;
        const InstructionDecodeResult decode = DecodeInstruction(instruction);
        event.instruction_size = decode.size;
        event.block_end = instruction + decode.size;
        std::memcpy(event.instruction_bytes, decode.bytes, sizeof(event.instruction_bytes));
        CaptureThreadContext(*context, event.thread_context);

        if (const auto *range = FindModuleRange(impl.module_ranges, instruction); range != nullptr)
        {
            event.inside_module = true;
            event.module_base = range->base;
            event.module_size = range->size;
            event.relative_instruction = instruction - range->base;
            if (!impl.lightweight_module_capture)
            {
                event.module_name = range->name;
            }
        }

        for (uint8_t index = 0; index < probe->capture_count && index < kProbeCaptureMaxCount; ++index)
        {
            FillProbeCapture(probe->captures[index], *context, event.probe_captures[event.probe_capture_count++]);
        }

        if (impl.options.callback != nullptr)
        {
            impl.options.callback(event, impl.options.callback_context);
        }

        return true;
    }
}
