#include "session_smoke_cli.h"
#include "session_smoke_trace_internal.h"
#include "core/heap_peek/VDTraceHeapPeekInternal.h"

#include <sstream>

namespace session_smoke
{
    namespace
    {
        std::wstring HexText(uintptr_t value)
        {
            std::wostringstream out;
            out << L"0x" << std::hex << value;
            return out.str();
        }

        bool DecodeInstructionOperands(
            const uint8_t *bytes,
            size_t size,
            ZydisDecodedInstruction &instruction,
            ZydisDecodedOperand *operands)
        {
            return vdtrace::heap_peek::DecodeHeapPeekInstruction(bytes, size, instruction, operands);
        }

        void RequireHeapPeekSizeFallback(const uint8_t *bytes, size_t size, uint8_t expected_size, const char *message)
        {
            ZydisDecodedInstruction instruction = {};
            ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT] = {};
            Require(DecodeInstructionOperands(bytes, size, instruction, operands), "failed to decode heap-peek sizing instruction");

            uint8_t memory_operand_index = 0xFF;
            for (uint8_t index = 0; index < instruction.operand_count_visible; ++index)
            {
                if (operands[index].type == ZYDIS_OPERAND_TYPE_MEMORY)
                {
                    memory_operand_index = index;
                    break;
                }
            }

            Require(memory_operand_index != 0xFF, "heap-peek sizing instruction had no memory operand");
            operands[memory_operand_index].size = 0;
            const uint8_t resolved_size = vdtrace::heap_peek::ResolveHeapPeekSize(instruction, operands, memory_operand_index, false);
            Require(resolved_size == expected_size, message);
        }

        std::wstring BuildObserverStepSpec()
        {
            return L"step@" + HexText(AnonymousExecCodeAddress())
                + L" steps=24 exit=return-or-leave";
        }

        std::wstring BuildObserverWriteSpec()
        {
            return L"write@" + HexText(AnonymousExecCodeAddress())
                + L" watch=" + HexText(AnonymousHeapBytesAddress()) + L":16:anonbuf"
                + L" steps=24 exit=return";
        }
    }

    void RunSessionSmokeTraceFeatureCases(const SessionSmokeSelection &selection, const SessionSmokeConfig &config)
    {
        if (ShouldRunCase(selection, "heap-extend"))
        {
            AnnounceSessionSmokeCase("heap-extend");
            const TraceCaseResult heap_extend_case = RunTraceCase(
                L"VDTraceSessionSmokeHeapExtend.log",
                MakeSessionSmokeOptions(&HeapExtendEntry, false, true, 1, vdtrace::FlowHitPolicy::EveryHit, 0));
            Require(Lowercase(heap_extend_case.log_text).find("[extend.mem]") != std::string::npos, "heap extend trace did not emit extender memory line");
            Require(Lowercase(heap_extend_case.log_text).find("origin=extend.") != std::string::npos, "heap extend trace did not route peek through extender");
            Require(Lowercase(heap_extend_case.log_text).find(" hex=0x") != std::string::npos, "heap extend trace did not emit folded integer hex");
        }

        if (ShouldRunCase(selection, "heap-peek-size"))
        {
            AnnounceSessionSmokeCase("heap-peek-size");
            const uint8_t xmm_load[] = {0xF3, 0x0F, 0x6F, 0x01};
            const uint8_t ymm_load[] = {0xC5, 0xFE, 0x6F, 0x01};
            RequireHeapPeekSizeFallback(xmm_load, sizeof(xmm_load), 16, "heap peek did not recover xmm width when operand size was absent");
            RequireHeapPeekSizeFallback(ymm_load, sizeof(ymm_load), 32, "heap peek did not recover ymm width when operand size was absent");
        }

        if (ShouldRunCase(selection, "static-window-sample"))
        {
            AnnounceSessionSmokeCase("static-window-sample");
            const TraceCaseResult static_window_case = RunTraceCase(
                L"VDTraceSessionSmokeStaticWindow.log",
                MakeSessionSmokeOptions(&StaticWindowSampleEntry, false, true, 1, vdtrace::FlowHitPolicy::EveryHit, 0));
            const std::string lowered = Lowercase(static_window_case.log_text);
            Require(lowered.find("[sample.static]") != std::string::npos, "static window sampling did not emit enhanced sample block");
            Require(lowered.find("section=.rdata") != std::string::npos, "static window sampling did not stay in .rdata");
            Require(lowered.find("size=1024") != std::string::npos, "static window sampling did not capture the full aligned 1kb window");
        }

        if (ShouldRunCase(selection, "static-refs"))
        {
            AnnounceSessionSmokeCase("static-refs");
            const TraceCaseResult static_ref_case = RunTraceCase(
                L"VDTraceSessionSmokeStaticRefs.log",
                MakeSessionSmokeOptions(&StaticRefEntry, false, true, 1, vdtrace::FlowHitPolicy::EveryHit, 0, true));
            Require(Lowercase(static_ref_case.log_text).find("[static] .rdata") != std::string::npos, "static reference analysis did not emit rdata hit");
            Require(Lowercase(static_ref_case.log_text).find("10 32 54 76 98 ba dc fe") != std::string::npos, "static reference analysis did not capture expected blob bytes");
            Require(Lowercase(static_ref_case.log_text).find("[static.ptr]") != std::string::npos, "static reference analysis did not emit pointer dereference");
            Require(Lowercase(static_ref_case.log_text).find("guess=ascii_string") != std::string::npos, "static reference analysis did not guess pointed string type");
            Require(Lowercase(static_ref_case.log_text).find(" hex=0x") != std::string::npos, "static reference analysis did not emit folded integer hex");
            Require(Lowercase(static_ref_case.log_text).find("vdtrace-static-pointer") != std::string::npos, "static reference analysis did not emit pointed string preview");
            Require(Lowercase(static_ref_case.static_refs_json_text).find("\"references\": [") != std::string::npos, "static reference json did not emit reference list");
            Require(Lowercase(static_ref_case.static_refs_json_text).find("\"slot_hex\":") != std::string::npos, "static reference json did not emit folded slot hex");
            Require(Lowercase(static_ref_case.static_refs_json_text).find("vdtrace-static-pointer") != std::string::npos, "static reference json did not emit pointed string preview");
            Require(Lowercase(static_ref_case.static_refs_json_text).find("\"instruction_label\":") != std::string::npos, "static reference json did not emit code location labels");
        }

        if (ShouldRunCase(selection, "value-probe"))
        {
            AnnounceSessionSmokeCase("value-probe");
            const TraceCaseResult probe_case = RunTraceCase(
                L"VDTraceSessionSmokeProbe.log",
                config.probe_options);
            Require(Lowercase(probe_case.log_text).find("kind=probe") != std::string::npos, "value probe trace did not emit probe event");
            Require(Lowercase(probe_case.log_text).find("[probe] keyiv=") != std::string::npos, "value probe trace did not emit keyiv capture");
            Require(Lowercase(probe_case.log_text).find("3a f1 8c 47 b2 09 6d ee 51 24 90 7c 18 d3 a4 62") != std::string::npos, "value probe trace did not capture the expected key bytes");
        }

        if (ShouldRunCase(selection, "observer-step"))
        {
            AnnounceSessionSmokeCase("observer-step");
            TraceRunOptions observer_step_options = MakeSessionSmokeOptions(&AnonymousExecEntry, false, true, 1, vdtrace::FlowHitPolicy::EveryHit, 0, true);
            observer_step_options.probe_spec = BuildObserverStepSpec();
            const TraceCaseResult observer_step_case = RunTraceCase(
                L"VDTraceSessionSmokeObserverStep.log",
                observer_step_options);
            Require(Lowercase(observer_step_case.log_text).find("kind=probe") != std::string::npos, "observer step trace did not emit probe events");
            Require(Lowercase(observer_step_case.log_text).find("[disasm] 0x") != std::string::npos, "observer step trace did not emit single-instruction disasm");
        }

        if (ShouldRunCase(selection, "observer-write"))
        {
            AnnounceSessionSmokeCase("observer-write");
            TraceRunOptions observer_write_options = MakeSessionSmokeOptions(&AnonymousExecEntry, false, true, 1, vdtrace::FlowHitPolicy::EveryHit, 0, true);
            observer_write_options.probe_spec = BuildObserverWriteSpec();
            const TraceCaseResult observer_write_case = RunTraceCase(
                L"VDTraceSessionSmokeObserverWrite.log",
                observer_write_options);
            Require(Lowercase(observer_write_case.log_text).find("[probe] anonbuf=") != std::string::npos, "observer write trace did not emit watched buffer capture");
            Require(Lowercase(observer_write_case.log_text).find("3b 5a 9d 47") != std::string::npos, "observer write trace did not capture the mutated anonymous buffer bytes");
        }

        if (ShouldRunCase(selection, "hot-loop-bypass"))
        {
            AnnounceSessionSmokeCase("hot-loop-bypass");
            const TraceCaseResult hot_loop_case = RunTraceCase(
                L"VDTraceSessionSmokeHotLoop.log",
                MakeSessionSmokeOptions(&HotLoopEntry, false, true, 4, vdtrace::FlowHitPolicy::FirstSeen, 0, false));
            const uint64_t hot_loop_steps = ParseStateCounter(hot_loop_case.state_text, L"steps=");
            Require(hot_loop_steps < 12000, "hot-loop bypass did not suppress repeated first-hit churn");
            Require(hot_loop_case.state_text.find(L"idle_escape=32") != std::wstring::npos, "hot-loop bypass state did not expose default idle-escape threshold");
        }

        if (ShouldRunCase(selection, "hot-loop-bypass-disabled"))
        {
            AnnounceSessionSmokeCase("hot-loop-bypass-disabled");
            const TraceCaseResult hot_loop_case = RunTraceCase(
                L"VDTraceSessionSmokeHotLoopBaseline.log",
                MakeSessionSmokeOptions(&HotLoopEntry, false, true, 4, vdtrace::FlowHitPolicy::FirstSeen, 0, false));
            const uint64_t hot_loop_steps = ParseStateCounter(hot_loop_case.state_text, L"steps=");
            TraceRunOptions hot_loop_disabled_options = MakeSessionSmokeOptions(&HotLoopEntry, false, true, 4, vdtrace::FlowHitPolicy::FirstSeen, 0, false);
            hot_loop_disabled_options.hot_bypass_threshold = 0;
            const TraceCaseResult hot_loop_disabled_case = RunTraceCase(
                L"VDTraceSessionSmokeHotLoopDisabled.log",
                hot_loop_disabled_options);
            const uint64_t hot_loop_disabled_steps = ParseStateCounter(hot_loop_disabled_case.state_text, L"steps=");
            Require(hot_loop_disabled_case.state_text.find(L"idle_escape=0") != std::wstring::npos, "disabled hot-loop bypass state did not expose zero threshold");
            Require(hot_loop_disabled_steps > hot_loop_steps + 5000, "disabling hot-loop bypass did not materially increase empty-spin steps");
        }

        if (ShouldRunCase(selection, "scene-hot-loop-bypass"))
        {
            AnnounceSessionSmokeCase("scene-hot-loop-bypass");
            const TraceCaseResult scene_hot_loop_case = RunTraceCase(
                L"VDTraceSessionSmokeSceneHotLoop.log",
                MakeSessionSmokeOptions(&SceneHotLoopEntry, false, true, 4, vdtrace::FlowHitPolicy::FirstSeen, 0, false));
            const uint64_t scene_hot_loop_steps = ParseStateCounter(scene_hot_loop_case.state_text, L"steps=");
            Require(scene_hot_loop_steps < 18000, "scene-shaped hot-loop bypass did not suppress repeated first-hit churn");
            Require(ParseStateCounter(scene_hot_loop_case.state_text, L"events=") != 0, "scene-shaped hot-loop trace produced no events");
        }

        if (ShouldRunCase(selection, "scene-hot-loop-rootstop"))
        {
            AnnounceSessionSmokeCase("scene-hot-loop-rootstop");
            const TraceCaseResult scene_hot_loop_rootstop_case = RunTraceCase(
                L"VDTraceSessionSmokeSceneHotLoopRootStop.log",
                MakeSessionSmokeOptions(&SceneHotLoopEntry, false, true, 4, vdtrace::FlowHitPolicy::FirstSeen, 0, true));
            Require(scene_hot_loop_rootstop_case.auto_stopped, "scene-shaped hot-loop rootstop did not auto-stop");
            const uint64_t scene_hot_loop_rootstop_steps = ParseStateCounter(scene_hot_loop_rootstop_case.state_text, L"steps=");
            Require(scene_hot_loop_rootstop_steps < 18000, "scene-shaped hot-loop rootstop still churned in repeated first-hit loop");
            Require(ParseStateCounter(scene_hot_loop_rootstop_case.state_text, L"events=") != 0, "scene-shaped hot-loop rootstop produced no events");
        }

        if (ShouldRunCase(selection, "max-events"))
        {
            AnnounceSessionSmokeCase("max-events");
            const TraceCaseResult max_events_case = RunTraceCase(
                L"VDTraceSessionSmokeMaxEvents.log",
                MakeSessionSmokeOptions(&UnityWorkerAssetEntry, true, true, 4, vdtrace::FlowHitPolicy::EveryHit, 4, false));
            Require(max_events_case.auto_stopped, "max-events trace did not auto-stop");
            Require(ParseStateCounter(max_events_case.state_text, L"events=") == 4, "max-events trace did not stop on the configured event count");
        }
    }
}
