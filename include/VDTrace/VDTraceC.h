#ifndef VDTRACE_VDTRACE_C_H
#define VDTRACE_VDTRACE_C_H

#include <Windows.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum VDTRACE_EVENT_KIND
    {
        VDTRACE_EVENT_KIND_UNKNOWN = 0,
        VDTRACE_EVENT_KIND_OTHER,
        VDTRACE_EVENT_KIND_PROBE,
        VDTRACE_EVENT_KIND_CALL,
        VDTRACE_EVENT_KIND_JUMP,
        VDTRACE_EVENT_KIND_CONDITIONAL_JUMP,
        VDTRACE_EVENT_KIND_RETURN,
        VDTRACE_EVENT_KIND_SYSCALL,
        VDTRACE_EVENT_KIND_INTERRUPT,
    } VDTRACE_EVENT_KIND;

    enum
    {
        VDTRACE_CALL_DEPTH_UNLIMITED = 0xFFFFFFFFu,
        VDTRACE_ENHANCED_SAMPLE_SLOT_COUNT = 2,
        VDTRACE_ENHANCED_SAMPLE_MAX_BYTES = 32,
        VDTRACE_ENHANCED_SAMPLE_SOURCE_RETURN_VALUE = 0xFFu,
        VDTRACE_PROBE_CAPTURE_MAX_COUNT = 4,
        VDTRACE_PROBE_LABEL_MAX_CHARS = 32
    };

    typedef enum VDTRACE_FLOW_HIT_POLICY
    {
        VDTRACE_FLOW_HIT_POLICY_FIRST_SEEN = 0,
        VDTRACE_FLOW_HIT_POLICY_EVERY_HIT = 1,
    } VDTRACE_FLOW_HIT_POLICY;

    typedef enum VDTRACE_TRACE_BACKEND
    {
        VDTRACE_TRACE_BACKEND_DR_CONTROL_FLOW = 0,
        VDTRACE_TRACE_BACKEND_TF_FULL_TRACE = 1,
        VDTRACE_TRACE_BACKEND_PT_CONTROL_FLOW = 2,
    } VDTRACE_TRACE_BACKEND;

    typedef struct VDTRACE_MODULE_RANGE
    {
        const wchar_t *name;
        uintptr_t base;
        size_t size;
    } VDTRACE_MODULE_RANGE;

    typedef struct VDTRACE_MEMORY_SAMPLE
    {
        bool valid;
        uint8_t source_index;
        uint8_t size;
        uintptr_t address;
        uint8_t bytes[VDTRACE_ENHANCED_SAMPLE_MAX_BYTES];
    } VDTRACE_MEMORY_SAMPLE;

    typedef struct VDTRACE_PROBE_CAPTURE
    {
        bool valid;
        bool has_bytes;
        uint8_t size;
        uintptr_t value;
        wchar_t label[VDTRACE_PROBE_LABEL_MAX_CHARS];
        uint8_t bytes[VDTRACE_ENHANCED_SAMPLE_MAX_BYTES];
    } VDTRACE_PROBE_CAPTURE;

    typedef struct VDTRACE_THREAD_CONTEXT
    {
        bool valid;
        uintptr_t rip;
        uintptr_t rsp;
        uintptr_t rbp;
        uintptr_t rax;
        uintptr_t rbx;
        uintptr_t rcx;
        uintptr_t rdx;
        uintptr_t rsi;
        uintptr_t rdi;
        uintptr_t r8;
        uintptr_t r9;
        uintptr_t r10;
        uintptr_t r11;
        uintptr_t r12;
        uintptr_t r13;
        uintptr_t r14;
        uintptr_t r15;
        uint64_t rflags;
    } VDTRACE_THREAD_CONTEXT;

    typedef struct VDTRACE_STEP_EVENT
    {
        uint64_t sequence;
        uint32_t thread_id;
        uintptr_t instruction;
        uintptr_t relative_instruction;
        uintptr_t module_base;
        uintptr_t stack_pointer;
        uintptr_t block_begin;
        uintptr_t block_end;
        uintptr_t target;
        uint32_t call_depth;
        size_t module_size;
        bool has_target;
        bool inside_module;
        bool minimal_record;
        VDTRACE_EVENT_KIND kind;
        uint8_t instruction_size;
        uint8_t instruction_bytes[16];
        const wchar_t *module_name;
        uint8_t call_argument_count;
        uintptr_t call_arguments[8];
        bool has_return_value;
        uintptr_t return_value;
        uint8_t memory_sample_count;
        VDTRACE_MEMORY_SAMPLE memory_samples[VDTRACE_ENHANCED_SAMPLE_SLOT_COUNT];
        bool has_return_memory_sample;
        VDTRACE_MEMORY_SAMPLE return_memory_sample;
        uint8_t probe_capture_count;
        VDTRACE_PROBE_CAPTURE probe_captures[VDTRACE_PROBE_CAPTURE_MAX_COUNT];
        VDTRACE_THREAD_CONTEXT thread_context;
    } VDTRACE_STEP_EVENT;

    typedef void (*VDTRACE_STEP_CALLBACK)(const VDTRACE_STEP_EVENT *event, void *context);

    const wchar_t *vdtrace_event_kind_name(VDTRACE_EVENT_KIND kind);

    typedef struct VDTRACE_OPTIONS
    {
        uint32_t thread_id;
        bool auto_select_thread;
        bool block_main_thread;
        bool queue_trigger_threads;
        const wchar_t **module_names;
        size_t module_name_count;
        uint64_t max_events;
        bool trace_outside_modules;
        VDTRACE_TRACE_BACKEND backend;
        bool control_flow_only;
        uint32_t max_call_depth;
        const wchar_t *depth_filter_spec;
        VDTRACE_FLOW_HIT_POLICY hit_policy;
        uint32_t hot_bypass_threshold;
        bool enhanced_sampling;
        const wchar_t *trigger_module_name;
        uintptr_t trigger_address;
        const wchar_t *probe_spec;
        bool stop_on_root_return;
        bool async_thread_handoff;
        VDTRACE_STEP_CALLBACK callback;
        void *callback_context;
    } VDTRACE_OPTIONS;

    typedef void *VDTRACE_SESSION_HANDLE;
    typedef void *VDTRACE_TEXT_RECORDER_HANDLE;

    VDTRACE_SESSION_HANDLE vdtrace_session_create(void);
    void vdtrace_session_destroy(VDTRACE_SESSION_HANDLE handle);
    bool vdtrace_session_configure(VDTRACE_SESSION_HANDLE handle, const VDTRACE_OPTIONS *options, wchar_t *error_buffer, size_t error_capacity);
    bool vdtrace_session_start(VDTRACE_SESSION_HANDLE handle, wchar_t *error_buffer, size_t error_capacity);
    bool vdtrace_session_stop(VDTRACE_SESSION_HANDLE handle, wchar_t *error_buffer, size_t error_capacity);
    bool vdtrace_session_is_configured(VDTRACE_SESSION_HANDLE handle);
    bool vdtrace_session_is_running(VDTRACE_SESSION_HANDLE handle);
    uint64_t vdtrace_session_event_count(VDTRACE_SESSION_HANDLE handle);

    VDTRACE_TEXT_RECORDER_HANDLE vdtrace_text_recorder_create(const wchar_t *path);
    void vdtrace_text_recorder_destroy(VDTRACE_TEXT_RECORDER_HANDLE handle);
    bool vdtrace_text_recorder_is_open(VDTRACE_TEXT_RECORDER_HANDLE handle);
    void vdtrace_text_recorder_callback(const VDTRACE_STEP_EVENT *event, void *context);
    void vdtrace_text_recorder_callback_ex(const VDTRACE_STEP_EVENT *event, size_t event_size, void *context);

#ifdef __cplusplus
}
#endif

#endif
