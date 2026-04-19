#include "pch.h"
#include "VDTraceFileSinkFormatInternal.h"

namespace vdtrace
{
    RecorderQueuedEvent MakeRecorderQueuedEvent(const StepEvent &event)
    {
        RecorderQueuedEvent queued = {};
        queued.sequence = event.sequence;
        queued.thread_id = event.thread_id;
        queued.instruction = event.instruction;
        queued.relative_instruction = event.relative_instruction;
        queued.module_base = event.module_base;
        queued.stack_pointer = event.stack_pointer;
        queued.block_begin = event.block_begin;
        queued.block_end = event.block_end;
        queued.target = event.target;
        queued.call_depth = event.call_depth;
        queued.module_size = event.module_size;
        queued.has_target = event.has_target;
        queued.inside_module = event.inside_module;
        queued.kind = event.kind;
        queued.instruction_size = event.instruction_size;
        std::memcpy(queued.instruction_bytes, event.instruction_bytes, sizeof(queued.instruction_bytes));
        queued.call_argument_count = event.call_argument_count;
        std::memcpy(queued.call_arguments, event.call_arguments, sizeof(queued.call_arguments));
        queued.has_return_value = event.has_return_value;
        queued.return_value = event.return_value;
        queued.memory_sample_count = event.memory_sample_count;
        std::memcpy(queued.memory_samples, event.memory_samples, sizeof(queued.memory_samples));
        queued.has_return_memory_sample = event.has_return_memory_sample;
        queued.return_memory_sample = event.return_memory_sample;
        queued.probe_capture_count = event.probe_capture_count;
        std::memcpy(queued.probe_captures, event.probe_captures, sizeof(queued.probe_captures));
        queued.thread_context = event.thread_context;
        if (!event.module_name.empty())
        {
            wcsncpy_s(queued.module_name, event.module_name.c_str(), _TRUNCATE);
        }
        queued.minimal_record = event.minimal_record;
        return queued;
    }

    std::string FormatRecorderEventLine(
        const RecorderQueuedEvent &event,
        uint64_t block_visit_count,
        std::unordered_map<uintptr_t, std::string> &module_name_cache,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map,
        std::unordered_map<uintptr_t, uint32_t> &dynamic_range_ids,
        const std::string &inline_suffix)
    {
        using namespace file_sink_format_detail;

        std::ostringstream out;
        out << "[tid=" << event.thread_id << "] "
            << ResolveModuleName(event, module_name_cache)
            << "!seq=" << event.sequence
            << " kind=" << NarrowKind(event.kind)
            << " depth=" << event.call_depth
            << " pass=" << std::dec << block_visit_count << std::hex
            << " rip=0x" << event.instruction
            << " rel=0x" << event.relative_instruction
            << " rsp=0x" << event.stack_pointer;
        if (event.has_target)
        {
            out << " target=" << ResolveAddressLabelText(event.target, address_label_cache, async_probe_map)
                << " target_abs=0x" << event.target;
        }
        if (event.has_return_value)
        {
            out << " retval=0x" << event.return_value;
        }
        if (event.memory_sample_count != 0)
        {
            out << " sample=" << std::dec << static_cast<unsigned>(event.memory_sample_count) << std::hex;
        }
        if (event.has_return_memory_sample)
        {
            out << " retmem=1";
        }
        if (event.probe_capture_count != 0)
        {
            out << " probe=" << std::dec << static_cast<unsigned>(event.probe_capture_count) << std::hex;
        }
        if (event.minimal_record)
        {
            out << " minimal=1";
        }

        out << " bytes=";
        for (uint8_t index = 0; index < event.instruction_size && index < sizeof(event.instruction_bytes); index++)
        {
            out << std::setw(2) << std::setfill('0') << std::hex << static_cast<unsigned>(event.instruction_bytes[index]);
        }

        if (!event.minimal_record)
        {
            const std::string disasm = FormatSingleProbeDisasm(event);
            if (!disasm.empty())
            {
                out << " [disasm] " << disasm;
            }
        }

        ExecutionRangeInfo source_info = {};
        ExecutionRangeInfo target_info = {};
        bool outside_jump_in = false;
        if (ShouldMarkTraceRangeJump(event, address_label_cache, async_probe_map, source_info, target_info, outside_jump_in))
        {
            out << " " << (outside_jump_in ? "[OUTSIDE_JUMP_IN]" : "[JUMP_OUT_OF_TRACE_RANGE]")
                << " from=" << source_info.text
                << " to=" << target_info.text;
            if (!outside_jump_in && target_info.kind == ExecutionRangeKind::AnonymousExecutable)
            {
                out << "[ENTER_DYNAMIC_MEMORY_" << std::dec << ResolveDynamicRangeId(dynamic_range_ids, target_info.identity) << std::hex << "]";
            }
            if (source_info.kind == ExecutionRangeKind::AnonymousExecutable)
            {
                out << "[LEAVE_DYNAMIC_MEMORY_" << std::dec << ResolveDynamicRangeId(dynamic_range_ids, source_info.identity) << std::hex << "]";
            }
        }
        if (!inline_suffix.empty())
        {
            out << inline_suffix;
        }

        out << std::setfill(' ') << "\n";
        out << FormatThreadContextBlock(event, "[ctx]");
        return out.str();
    }

    bool CanSummarizeOutsideOtherEvent(const RecorderQueuedEvent &event)
    {
        return !event.inside_module
            && !event.minimal_record
            && event.kind == EventKind::Other;
    }

    std::string FormatOutsideOtherRunBlock(
        const RecorderQueuedEvent &first,
        const RecorderQueuedEvent &last,
        size_t count,
        uint64_t first_block_visit_count,
        uint64_t last_block_visit_count,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map)
    {
        using namespace file_sink_format_detail;

        const uintptr_t block_begin = first.block_begin != 0 ? first.block_begin : first.instruction;
        const uintptr_t block_end = last.block_end != 0 ? last.block_end : (last.instruction + last.instruction_size);
        const ExecutionRangeInfo range_info = DescribeExecutionRange(first.instruction, address_label_cache, async_probe_map);

        std::ostringstream out;
        out << "[tid=" << first.thread_id << "] outside!seq=" << first.sequence;
        if (count > 1)
        {
            out << ".." << last.sequence;
        }
        out << " kind=other.run"
            << " depth=" << first.call_depth
            << " pass=" << std::dec << first_block_visit_count;
        if (last_block_visit_count != first_block_visit_count)
        {
            out << ".." << last_block_visit_count;
        }
        out << " count=" << count
            << std::hex
            << " block=0x" << block_begin << "-0x" << block_end
            << " rip=0x" << first.instruction;
        if (last.instruction != first.instruction)
        {
            out << "..0x" << last.instruction;
        }
        out << " rsp=0x" << first.stack_pointer;
        if (last.stack_pointer != first.stack_pointer)
        {
            out << "->0x" << last.stack_pointer;
        }
        if (!range_info.text.empty())
        {
            out << " region=" << range_info.text;
        }
        out << "\n";

        const std::string first_disasm = FormatSingleProbeDisasm(first);
        if (!first_disasm.empty())
        {
            out << "   [first] " << first_disasm << "\n";
        }

        if (count > 1 && last.instruction != first.instruction)
        {
            const std::string last_disasm = FormatSingleProbeDisasm(last);
            if (!last_disasm.empty())
            {
                out << "   [last] " << last_disasm << "\n";
            }
        }

        out << FormatThreadContextBlock(last, "[ctx.last]");
        return out.str();
    }

    std::string FormatFridaStyleEventBlock(
        const RecorderQueuedEvent &event,
        std::unordered_map<uintptr_t, std::string> &address_label_cache,
        const std::unordered_map<uintptr_t, ResolvedAsyncProbe> &async_probe_map,
        std::unordered_map<DWORD, std::vector<ActiveCallFrame>> &active_calls,
        std::unordered_set<uintptr_t> &previewed_functions,
        const TextFileRecorderHeapPeek *heap_peek)
    {
        using namespace file_sink_format_detail;

        auto &thread_stack = active_calls[event.thread_id];
        while (!thread_stack.empty() && thread_stack.back().depth > event.call_depth)
        {
            thread_stack.pop_back();
        }

        if (event.minimal_record)
        {
            return {};
        }

        std::ostringstream out;
        if (event.kind == EventKind::Call && event.has_target)
        {
            const std::string indent = CallIndent(event.call_depth);
            const std::string summary = BuildCallSummaryText(event, address_label_cache, async_probe_map);
            if (!summary.empty())
            {
                out << indent << summary << "\n";
            }

            if (previewed_functions.insert(event.target).second)
            {
                const std::string preview = BuildFunctionPreviewText(event, address_label_cache, async_probe_map, heap_peek);
                out << PrefixMultiline(preview, indent + "   ");
            }

            ActiveCallFrame frame = {};
            frame.depth = event.call_depth + 1;
            frame.target = event.target;
            frame.display_name = ResolveAddressLabelText(event.target, address_label_cache, async_probe_map);
            RememberEnhancedSamplingFrame(frame, event);
            out << FormatEnhancedSamplingEntryBlock(event, indent + "   ");
            thread_stack.push_back(std::move(frame));
            return out.str();
        }

        if (event.kind == EventKind::Probe)
        {
            out << FormatProbeBlock(event, CallIndent(event.call_depth));
            return out.str();
        }

        if (event.kind == EventKind::Return)
        {
            std::string callee_name;
            ActiveCallFrame matched_frame = {};
            bool have_frame = false;
            if (!thread_stack.empty())
            {
                const ActiveCallFrame frame = thread_stack.back();
                if (frame.depth == event.call_depth)
                {
                    callee_name = frame.display_name;
                    matched_frame = frame;
                    have_frame = true;
                    thread_stack.pop_back();
                }
            }

            const std::string indent = CallIndent(event.call_depth > 0 ? event.call_depth - 1 : 0);
            out << indent << "<= ";
            if (event.has_return_value)
            {
                out << FormatValueWithPreview(event.return_value, false, address_label_cache, async_probe_map);
            }
            else
            {
                out << "?";
            }
            if (!callee_name.empty())
            {
                out << " // " << callee_name;
            }
            out << "\n";
            out << FormatEnhancedSamplingReturnBlock(event, indent + "   ", have_frame ? &matched_frame : nullptr);
            return out.str();
        }

        return {};
    }
}
