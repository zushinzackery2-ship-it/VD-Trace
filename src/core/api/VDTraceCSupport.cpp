#include "pch.h"
#include "core/api/VDTraceCInternal.h"

namespace vdtrace::c_detail
{
    void CopyMemorySampleToC(const vdtrace::MemorySample &source, VDTRACE_MEMORY_SAMPLE &destination)
    {
        destination.valid = source.valid;
        destination.source_index = source.source_index;
        destination.size = source.size;
        destination.address = source.address;
        std::memcpy(destination.bytes, source.bytes, sizeof(destination.bytes));
    }

    void CopyMemorySampleFromC(const VDTRACE_MEMORY_SAMPLE &source, vdtrace::MemorySample &destination)
    {
        destination.valid = source.valid;
        destination.source_index = source.source_index;
        destination.size = source.size;
        destination.address = source.address;
        std::memcpy(destination.bytes, source.bytes, sizeof(destination.bytes));
    }

    void CopyProbeCaptureToC(const vdtrace::ProbeCapture &source, VDTRACE_PROBE_CAPTURE &destination)
    {
        destination.valid = source.valid;
        destination.has_bytes = source.has_bytes;
        destination.size = source.size;
        destination.value = source.value;
        std::wmemset(destination.label, 0, std::size(destination.label));
        wcsncpy_s(destination.label, source.label, _TRUNCATE);
        std::memcpy(destination.bytes, source.bytes, sizeof(destination.bytes));
    }

    void CopyProbeCaptureFromC(const VDTRACE_PROBE_CAPTURE &source, vdtrace::ProbeCapture &destination)
    {
        destination.valid = source.valid;
        destination.has_bytes = source.has_bytes;
        destination.size = source.size;
        destination.value = source.value;
        std::wmemset(destination.label, 0, std::size(destination.label));
        wcsncpy_s(destination.label, source.label, _TRUNCATE);
        std::memcpy(destination.bytes, source.bytes, sizeof(destination.bytes));
    }

    void CCallbackBridge(const vdtrace::StepEvent &event, void *context)
    {
        auto *handle = static_cast<CSessionHandle *>(context);
        if (handle == nullptr || handle->callback == nullptr)
        {
            return;
        }

        VDTRACE_STEP_EVENT c_event = {};
        vdtrace::CopyEventToCStruct(event, c_event);
        handle->callback(&c_event, handle->callback_context);
    }
}

namespace vdtrace
{
    void StoreErrorString(const std::wstring &error, wchar_t *buffer, size_t capacity)
    {
        if (buffer == nullptr || capacity == 0)
        {
            return;
        }

        wcsncpy_s(buffer, capacity, error.c_str(), _TRUNCATE);
    }

    void CopyEventToCStruct(const StepEvent &event, VDTRACE_STEP_EVENT &out)
    {
        out = {};
        out.sequence = event.sequence;
        out.thread_id = event.thread_id;
        out.instruction = event.instruction;
        out.relative_instruction = event.relative_instruction;
        out.module_base = event.module_base;
        out.stack_pointer = event.stack_pointer;
        out.block_begin = event.block_begin;
        out.block_end = event.block_end;
        out.target = event.target;
        out.call_depth = event.call_depth;
        out.module_size = event.module_size;
        out.has_target = event.has_target;
        out.inside_module = event.inside_module;
        out.minimal_record = event.minimal_record;
        out.kind = static_cast<VDTRACE_EVENT_KIND>(event.kind);
        out.instruction_size = event.instruction_size;
        std::memcpy(out.instruction_bytes, event.instruction_bytes, sizeof(out.instruction_bytes));
        out.module_name = event.module_name.c_str();
        out.call_argument_count = event.call_argument_count;
        std::memcpy(out.call_arguments, event.call_arguments, sizeof(out.call_arguments));
        out.has_return_value = event.has_return_value;
        out.return_value = event.return_value;
        out.memory_sample_count = event.memory_sample_count;
        for (size_t index = 0; index < std::size(out.memory_samples); ++index)
        {
            c_detail::CopyMemorySampleToC(event.memory_samples[index], out.memory_samples[index]);
        }
        out.has_return_memory_sample = event.has_return_memory_sample;
        c_detail::CopyMemorySampleToC(event.return_memory_sample, out.return_memory_sample);
        out.probe_capture_count = event.probe_capture_count;
        for (size_t index = 0; index < std::size(out.probe_captures); ++index)
        {
            c_detail::CopyProbeCaptureToC(event.probe_captures[index], out.probe_captures[index]);
        }
        out.thread_context.valid = event.thread_context.valid;
        out.thread_context.rip = event.thread_context.rip;
        out.thread_context.rsp = event.thread_context.rsp;
        out.thread_context.rbp = event.thread_context.rbp;
        out.thread_context.rax = event.thread_context.rax;
        out.thread_context.rbx = event.thread_context.rbx;
        out.thread_context.rcx = event.thread_context.rcx;
        out.thread_context.rdx = event.thread_context.rdx;
        out.thread_context.rsi = event.thread_context.rsi;
        out.thread_context.rdi = event.thread_context.rdi;
        out.thread_context.r8 = event.thread_context.r8;
        out.thread_context.r9 = event.thread_context.r9;
        out.thread_context.r10 = event.thread_context.r10;
        out.thread_context.r11 = event.thread_context.r11;
        out.thread_context.r12 = event.thread_context.r12;
        out.thread_context.r13 = event.thread_context.r13;
        out.thread_context.r14 = event.thread_context.r14;
        out.thread_context.r15 = event.thread_context.r15;
        out.thread_context.rflags = event.thread_context.rflags;
    }
}
