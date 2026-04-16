#include "pch.h"
#include "VDTraceInternal.h"
#include "VDTraceCInternal.h"

extern "C"
{
    using SessionHandle = vdtrace::c_detail::CSessionHandle *;
    using RecorderHandle = vdtrace::TextFileRecorder *;

    const wchar_t *vdtrace_event_kind_name(VDTRACE_EVENT_KIND kind)
    {
        return vdtrace::EventKindName(static_cast<vdtrace::EventKind>(kind));
    }

    VDTRACE_SESSION_HANDLE vdtrace_session_create(void)
    {
        return new (std::nothrow) vdtrace::c_detail::CSessionHandle();
    }

    void vdtrace_session_destroy(VDTRACE_SESSION_HANDLE handle)
    {
        delete static_cast<SessionHandle>(handle);
    }

    bool vdtrace_session_configure(VDTRACE_SESSION_HANDLE handle, const VDTRACE_OPTIONS *options, wchar_t *error_buffer, size_t error_capacity)
    {
        if (handle == nullptr || options == nullptr)
        {
            vdtrace::StoreErrorString(L"session 或 options 为空。", error_buffer, error_capacity);
            return false;
        }

        vdtrace::Options native_options = {};
        native_options.thread_id = options->thread_id;
        native_options.auto_select_thread = options->auto_select_thread;
        native_options.block_main_thread = options->block_main_thread;
        native_options.queue_trigger_threads = options->queue_trigger_threads;
        native_options.max_events = options->max_events;
        native_options.trace_outside_modules = options->trace_outside_modules;
        native_options.backend = static_cast<vdtrace::TraceBackend>(options->backend);
        if (native_options.backend == vdtrace::TraceBackend::DrControlFlow && !options->control_flow_only)
        {
            native_options.backend = vdtrace::TraceBackend::TfFullTrace;
        }
        native_options.control_flow_only = options->control_flow_only;
        native_options.max_call_depth = options->max_call_depth;
        if (options->depth_filter_spec != nullptr)
        {
            native_options.depth_filter_spec = options->depth_filter_spec;
        }
        native_options.hit_policy = static_cast<vdtrace::FlowHitPolicy>(options->hit_policy);
        native_options.hot_bypass_threshold = options->hot_bypass_threshold;
        native_options.enhanced_sampling = options->enhanced_sampling;
        if (options->trigger_module_name != nullptr)
        {
            native_options.trigger_module_name = options->trigger_module_name;
        }
        native_options.trigger_address = options->trigger_address;
        if (options->probe_spec != nullptr)
        {
            native_options.probe_spec = options->probe_spec;
        }
        native_options.stop_on_root_return = options->stop_on_root_return;
        native_options.async_thread_handoff = options->async_thread_handoff;

        SessionHandle session_handle = static_cast<SessionHandle>(handle);
        session_handle->callback = options->callback;
        session_handle->callback_context = options->callback_context;
        if (session_handle->callback != nullptr)
        {
            native_options.callback = vdtrace::c_detail::CCallbackBridge;
            native_options.callback_context = session_handle;
        }

        for (size_t i = 0; i < options->module_name_count; i++)
        {
            if (options->module_names != nullptr && options->module_names[i] != nullptr)
            {
                native_options.module_names.emplace_back(options->module_names[i]);
            }
        }

        std::wstring error;
        const bool success = session_handle->session.Configure(native_options, error);
        vdtrace::StoreErrorString(error, error_buffer, error_capacity);
        return success;
    }

    bool vdtrace_session_start(VDTRACE_SESSION_HANDLE handle, wchar_t *error_buffer, size_t error_capacity)
    {
        if (handle == nullptr)
        {
            vdtrace::StoreErrorString(L"session 为空。", error_buffer, error_capacity);
            return false;
        }

        std::wstring error;
        const bool success = static_cast<SessionHandle>(handle)->session.Start(error);
        vdtrace::StoreErrorString(error, error_buffer, error_capacity);
        return success;
    }

    bool vdtrace_session_stop(VDTRACE_SESSION_HANDLE handle, wchar_t *error_buffer, size_t error_capacity)
    {
        if (handle == nullptr)
        {
            vdtrace::StoreErrorString(L"session 为空。", error_buffer, error_capacity);
            return false;
        }

        std::wstring error;
        const bool success = static_cast<SessionHandle>(handle)->session.Stop(error);
        vdtrace::StoreErrorString(error, error_buffer, error_capacity);
        return success;
    }

    bool vdtrace_session_is_configured(VDTRACE_SESSION_HANDLE handle)
    {
        return handle != nullptr && static_cast<SessionHandle>(handle)->session.IsConfigured();
    }

    bool vdtrace_session_is_running(VDTRACE_SESSION_HANDLE handle)
    {
        return handle != nullptr && static_cast<SessionHandle>(handle)->session.IsRunning();
    }

    uint64_t vdtrace_session_event_count(VDTRACE_SESSION_HANDLE handle)
    {
        return handle != nullptr ? static_cast<SessionHandle>(handle)->session.EventCount() : 0;
    }

    VDTRACE_TEXT_RECORDER_HANDLE vdtrace_text_recorder_create(const wchar_t *path)
    {
        if (path == nullptr)
        {
            return nullptr;
        }

        return new (std::nothrow) vdtrace::TextFileRecorder(path);
    }

    void vdtrace_text_recorder_destroy(VDTRACE_TEXT_RECORDER_HANDLE handle)
    {
        delete static_cast<RecorderHandle>(handle);
    }

    bool vdtrace_text_recorder_is_open(VDTRACE_TEXT_RECORDER_HANDLE handle)
    {
        return handle != nullptr && static_cast<RecorderHandle>(handle)->IsOpen();
    }

    void vdtrace_text_recorder_callback(const VDTRACE_STEP_EVENT *event, void *context)
    {
        vdtrace_text_recorder_callback_ex(event, sizeof(VDTRACE_STEP_EVENT), context);
    }

    void vdtrace_text_recorder_callback_ex(const VDTRACE_STEP_EVENT *event, size_t event_size, void *context)
    {
        auto *recorder = static_cast<RecorderHandle>(context);
        if (event == nullptr || recorder == nullptr)
        {
            return;
        }

        vdtrace::StepEvent native_event = {};
        native_event.sequence = event->sequence;
        native_event.thread_id = event->thread_id;
        native_event.instruction = event->instruction;
        native_event.relative_instruction = event->relative_instruction;
        native_event.module_base = event->module_base;
        native_event.stack_pointer = event->stack_pointer;
        native_event.block_begin = event->block_begin;
        native_event.block_end = event->block_end;
        native_event.target = event->target;
        native_event.call_depth = event->call_depth;
        native_event.module_size = event->module_size;
        native_event.has_target = event->has_target;
        native_event.inside_module = event->inside_module;
        native_event.minimal_record = event->minimal_record;
        native_event.kind = static_cast<vdtrace::EventKind>(event->kind);
        native_event.instruction_size = event->instruction_size;
        std::memcpy(native_event.instruction_bytes, event->instruction_bytes, sizeof(native_event.instruction_bytes));
        if (event->module_name != nullptr)
        {
            native_event.module_name = event->module_name;
        }
        if (event_size >= sizeof(VDTRACE_STEP_EVENT))
        {
            native_event.call_argument_count = event->call_argument_count;
            std::memcpy(native_event.call_arguments, event->call_arguments, sizeof(native_event.call_arguments));
            native_event.has_return_value = event->has_return_value;
            native_event.return_value = event->return_value;
            native_event.memory_sample_count = event->memory_sample_count;
            for (size_t index = 0; index < std::size(native_event.memory_samples); ++index)
            {
                vdtrace::c_detail::CopyMemorySampleFromC(event->memory_samples[index], native_event.memory_samples[index]);
            }
            native_event.has_return_memory_sample = event->has_return_memory_sample;
            vdtrace::c_detail::CopyMemorySampleFromC(event->return_memory_sample, native_event.return_memory_sample);
            native_event.probe_capture_count = event->probe_capture_count;
            for (size_t index = 0; index < std::size(native_event.probe_captures); ++index)
            {
                vdtrace::c_detail::CopyProbeCaptureFromC(event->probe_captures[index], native_event.probe_captures[index]);
            }
            native_event.thread_context.valid = event->thread_context.valid;
            native_event.thread_context.rip = event->thread_context.rip;
            native_event.thread_context.rsp = event->thread_context.rsp;
            native_event.thread_context.rbp = event->thread_context.rbp;
            native_event.thread_context.rax = event->thread_context.rax;
            native_event.thread_context.rbx = event->thread_context.rbx;
            native_event.thread_context.rcx = event->thread_context.rcx;
            native_event.thread_context.rdx = event->thread_context.rdx;
            native_event.thread_context.rsi = event->thread_context.rsi;
            native_event.thread_context.rdi = event->thread_context.rdi;
            native_event.thread_context.r8 = event->thread_context.r8;
            native_event.thread_context.r9 = event->thread_context.r9;
            native_event.thread_context.r10 = event->thread_context.r10;
            native_event.thread_context.r11 = event->thread_context.r11;
            native_event.thread_context.r12 = event->thread_context.r12;
            native_event.thread_context.r13 = event->thread_context.r13;
            native_event.thread_context.r14 = event->thread_context.r14;
            native_event.thread_context.r15 = event->thread_context.r15;
            native_event.thread_context.rflags = event->thread_context.rflags;
        }

        recorder->OnStep(native_event);
    }
}
