#include "session_smoke_cli.h"
#include "session_smoke_trace_internal.h"
#include "core/recorder/VDTraceFileSinkInternal.h"

#include <atomic>
#include <filesystem>
#include <system_error>
#include <thread>
#include <vector>

namespace session_smoke
{
    namespace
    {
        vdtrace::StepEvent BuildWriteAccountingEvent(uint64_t sequence, bool minimal_record)
        {
            vdtrace::StepEvent event = {};
            event.sequence = sequence;
            event.thread_id = GetCurrentThreadId();
            event.instruction = 0x180000000ull + static_cast<uintptr_t>(sequence * 4u);
            event.relative_instruction = static_cast<uintptr_t>(sequence * 4u);
            event.module_base = 0x180000000ull;
            event.module_size = 0x1000;
            event.block_begin = event.instruction;
            event.block_end = event.instruction + 1;
            event.kind = vdtrace::EventKind::Other;
            event.instruction_size = 1;
            event.instruction_bytes[0] = 0x90;
            event.module_name = L"write-accounting";
            event.minimal_record = minimal_record;
            return event;
        }

        void WaitForRecorderDrain(vdtrace::TextFileRecorder &recorder, DWORD timeout_ms, const char *timeout_message)
        {
            const ULONGLONG deadline = GetTickCount64() + timeout_ms;
            while (recorder.IsWriting() && GetTickCount64() < deadline)
            {
                Sleep(1);
            }

            Require(!recorder.IsWriting(), timeout_message);
        }
    }

    void RunSessionSmokeRecorderCases(const SessionSmokeSelection &selection)
    {
        if (ShouldRunCase(selection, "write-accounting"))
        {
            AnnounceSessionSmokeCase("write-accounting");
            const std::filesystem::path log_path = detail::GetExecutableDirectory() / L"VDTraceSessionSmokeWriteAccounting.log";
            std::error_code ignored_error;
            std::filesystem::remove(log_path, ignored_error);
            const uint64_t expected_events = 43000;
            {
                vdtrace::TextFileRecorder recorder(log_path.wstring());
                Require(recorder.IsOpen(), "write-accounting recorder failed to open");
                for (uint64_t index = 0; index < expected_events; ++index)
                {
                    recorder.OnStep(BuildWriteAccountingEvent(index + 1, true));
                }

                WaitForRecorderDrain(recorder, 30000, "write-accounting recorder did not drain");
                Require(recorder.PendingWriteBytes() == 0, "write-accounting recorder still had pending write bytes");
                Require(recorder.PendingWriteEventCount() == 0, "write-accounting recorder still had pending file writes");
                Require(recorder.WrittenEventCount() == expected_events, "write-accounting recorder did not persist every recorded event");
                Require(recorder.DroppedEventCount() == 0, "write-accounting recorder dropped ring events");
                Require(recorder.DroppedWriteEventCount() == 0, "write-accounting recorder dropped write events");
            }

            Require(std::filesystem::exists(log_path), "write-accounting log missing");
            Require(std::filesystem::file_size(log_path) != 0, "write-accounting recorder produced no log");
        }

        if (ShouldRunCase(selection, "write-accounting-pressure"))
        {
            AnnounceSessionSmokeCase("write-accounting-pressure");
            const std::filesystem::path log_path = detail::GetExecutableDirectory() / L"VDTraceSessionSmokeWriteAccountingPressure.log";
            std::error_code ignored_error;
            std::filesystem::remove(log_path, ignored_error);

            constexpr size_t kProducerCount = 4;
            const uint64_t events_per_producer = (vdtrace::kRecorderRingCapacity / kProducerCount) + 32768;
            const uint64_t expected_events = events_per_producer * kProducerCount;
            {
                vdtrace::TextFileRecorder recorder(log_path.wstring());
                Require(recorder.IsOpen(), "write-accounting-pressure recorder failed to open");

                std::atomic<uint64_t> sequence = 0;
                std::vector<std::thread> producers;
                producers.reserve(kProducerCount);
                for (size_t index = 0; index < kProducerCount; ++index)
                {
                    producers.emplace_back(
                        [&recorder, &sequence, events_per_producer]()
                        {
                            for (uint64_t event_index = 0; event_index < events_per_producer; ++event_index)
                            {
                                const uint64_t next_sequence = sequence.fetch_add(1, std::memory_order_relaxed) + 1;
                                recorder.OnStep(BuildWriteAccountingEvent(next_sequence, false));
                            }
                        });
                }

                for (std::thread &producer : producers)
                {
                    producer.join();
                }

                WaitForRecorderDrain(recorder, 60000, "write-accounting-pressure recorder did not drain");
                Require(recorder.PendingWriteBytes() == 0, "write-accounting-pressure recorder still had pending write bytes");
                Require(recorder.PendingWriteEventCount() == 0, "write-accounting-pressure recorder still had pending file writes");
                Require(recorder.WrittenEventCount() == expected_events, "write-accounting-pressure recorder did not persist every recorded event");
                Require(recorder.DroppedEventCount() == 0, "write-accounting-pressure recorder dropped ring events");
                Require(recorder.DroppedWriteEventCount() == 0, "write-accounting-pressure recorder dropped write events");
            }

            Require(std::filesystem::exists(log_path), "write-accounting-pressure log missing");
            Require(std::filesystem::file_size(log_path) != 0, "write-accounting-pressure recorder produced no log");
        }
    }
}
