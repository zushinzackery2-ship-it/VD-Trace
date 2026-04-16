#include "pch.h"
#include "agent/VDTraceAgentMemory.h"
#include "agent/VDTraceAgentState.h"
#include "agent/VDTraceAgentStateInternal.h"
#include "agent/VDTraceAgentDump.h"

namespace vdtrace::agent
{
    State &State::Instance()
    {
        static State instance;
        return instance;
    }

    bool State::Configure(const IpcConfigurePayload &payload, std::string &message)
    {
        std::lock_guard<std::mutex> lock(lock_);

        has_valid_configuration_ = false;
        std::wstring ignored_error;
        session_.Stop(ignored_error);
        recorder_.reset();

        vdtrace::Options options = {};
        options.thread_id = static_cast<DWORD>(payload.thread_id);
        options.module_names = ParseModuleNames(payload.module_names);
        options.max_events = payload.max_events;
        options.trace_outside_modules = payload.trace_outside_modules != 0;
        options.backend = static_cast<TraceBackend>(payload.backend);
        if (options.backend != TraceBackend::DrControlFlow
            && options.backend != TraceBackend::TfFullTrace)
        {
            message = "backend only supports dr/tf";
            return false;
        }
        if (options.backend == TraceBackend::DrControlFlow && payload.control_flow_only == 0)
        {
            options.backend = TraceBackend::TfFullTrace;
        }
        options.control_flow_only = payload.control_flow_only != 0;
        options.max_call_depth = payload.max_call_depth;
        options.depth_filter_spec = WidenUtf8(payload.depth_filter_spec);
        options.hit_policy = state_detail::NormalizeHitPolicy(payload.hit_policy);
        options.hot_bypass_threshold = payload.hot_bypass_threshold;
        options.enhanced_sampling = payload.enhanced_sampling != 0;
        options.auto_select_thread = payload.auto_select_thread != 0;
        options.block_main_thread = payload.block_main_thread != 0;
        options.queue_trigger_threads = payload.queue_trigger_threads != 0;
        if (!ParseTriggerPoint(payload.trigger_point, options.trigger_module_name, options.trigger_address, message))
        {
            return false;
        }
        options.probe_spec = WidenUtf8(payload.probe_spec);
        options.stop_on_root_return = payload.stop_on_root_return != 0;
        options.async_thread_handoff = payload.async_thread_handoff != 0;

        output_path_ = NormalizeOutputPath(WidenUtf8(payload.output_path));
        if (output_path_.empty())
        {
            output_path_ = NormalizeOutputPath(L"");
        }
        options.output_path = output_path_;

        const auto parent = std::filesystem::path(output_path_).parent_path();
        if (!parent.empty())
        {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
        }

        recorder_ = std::make_unique<vdtrace::TextFileRecorder>(output_path_, options);
        if (!recorder_->IsOpen())
        {
            message = "failed to open output file: " + NarrowUtf8(output_path_);
            recorder_.reset();
            return false;
        }

        options.callback = vdtrace::TextFileRecorder::Callback;
        options.callback_context = recorder_.get();

        std::wstring error;
        if (!session_.Configure(options, error))
        {
            message = NarrowUtf8(error);
            recorder_.reset();
            return false;
        }

        has_valid_configuration_ = true;
        message = "configured";
        return true;
    }

    bool State::Start(std::string &message)
    {
        std::lock_guard<std::mutex> lock(lock_);
        if (!has_valid_configuration_)
        {
            message = "trace session is not configured";
            return false;
        }

        std::wstring error;
        if (!session_.Start(error))
        {
            message = NarrowUtf8(error);
            return false;
        }

        message = "started";
        return true;
    }

    bool State::Stop(std::string &message)
    {
        std::lock_guard<std::mutex> lock(lock_);
        std::wstring error;
        if (!session_.Stop(error))
        {
            message = NarrowUtf8(error);
            return false;
        }

        recorder_.reset();
        message = "stopped";
        return true;
    }

    std::string State::Status() const
    {
        std::lock_guard<std::mutex> lock(lock_);
        std::string status = NarrowUtf8(session_.DescribeState());
        const uint64_t total_events = session_.EventCount();
        const bool writing = recorder_ != nullptr && recorder_->IsWriting();
        const size_t pending_events = recorder_ != nullptr ? recorder_->PendingEventCount() : 0;
        const size_t pending_write_bytes = recorder_ != nullptr ? recorder_->PendingWriteBytes() : 0;
        const uint64_t pending_write_events = recorder_ != nullptr ? recorder_->PendingWriteEventCount() : 0;
        const uint64_t written_events = recorder_ != nullptr ? recorder_->WrittenEventCount() : 0;
        const uint64_t dropped_events = recorder_ != nullptr ? recorder_->DroppedEventCount() : 0;
        const uint64_t dropped_write_events = recorder_ != nullptr ? recorder_->DroppedWriteEventCount() : 0;
        const uint64_t accounted_events = written_events
            + static_cast<uint64_t>(pending_events)
            + pending_write_events
            + dropped_events
            + dropped_write_events;
        const uint64_t event_gap = total_events > accounted_events ? total_events - accounted_events : 0;
        status += writing ? " writing=1" : " writing=0";
        status += " pending_events=" + std::to_string(pending_events);
        status += " pending_write_bytes=" + std::to_string(pending_write_bytes);
        status += " pending_write_events=" + std::to_string(pending_write_events);
        status += " written_events=" + std::to_string(written_events);
        status += " dropped_events_total=" + std::to_string(dropped_events);
        status += " dropped_write_events=" + std::to_string(dropped_write_events);
        status += " accounted_events=" + std::to_string(accounted_events);
        status += " event_gap=" + std::to_string(event_gap);
        return status;
    }

    bool State::ListModules(bool include_system_modules, std::string &message) const
    {
        std::lock_guard<std::mutex> lock(lock_);
        return BuildLoadedModuleList(include_system_modules, message);
    }

    bool State::DumpModule(const char *module_name, const char *output_directory, std::string &message) const
    {
        std::lock_guard<std::mutex> lock(lock_);
        return DumpModuleToDirectory(WidenUtf8(module_name), WidenUtf8(output_directory), message);
    }

    bool State::ReadMemory(const char *address_text, uint32_t size, std::string &message) const
    {
        std::lock_guard<std::mutex> lock(lock_);
        return ReadMemoryText(address_text, size, message);
    }

    bool State::WriteMemory(const char *address_text, const uint8_t *bytes, uint32_t size, std::string &message) const
    {
        std::lock_guard<std::mutex> lock(lock_);
        return WriteMemoryText(address_text, bytes, size, message);
    }
}
