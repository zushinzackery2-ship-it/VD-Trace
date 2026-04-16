#include "session_smoke_cli.h"

namespace session_smoke
{
    void RunSessionSmokeTraceBasicCases(const SessionSmokeSelection &selection, const SessionSmokeConfig &config)
    {
        if (ShouldRunCase(selection, "same-level"))
        {
            AnnounceSessionSmokeCase("same-level");
            const TraceCaseResult same_level_case = RunTraceCase(
                L"VDTraceSessionSmokeSameLevel.log",
                MakeSessionSmokeOptions(&SameLevelEntry, false, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0));
            Require(same_level_case.log_text.find("depth=1") == std::string::npos, "same-level trace entered nested call depth");
            Require(same_level_case.log_text.find("[fn]") != std::string::npos, "same-level trace did not emit function preview");
            Require(Lowercase(same_level_case.log_text).find("ret\n") != std::string::npos, "same-level trace did not emit return instruction disassembly");
            Require(same_level_case.log_text.find(" pass=") != std::string::npos, "same-level trace did not emit block pass count");
            Require(Lowercase(same_level_case.log_text).find("[ctx] rip=0x") != std::string::npos, "same-level trace did not emit thread context block");
            Require(Lowercase(same_level_case.log_text).find("rax=0x") != std::string::npos, "same-level trace did not emit register context");
        }

        if (ShouldRunCase(selection, "repeat"))
        {
            AnnounceSessionSmokeCase("repeat");
            const TraceCaseResult repeat_first_case = RunTraceCase(
                L"VDTraceSessionSmokeRepeatFirst.log",
                MakeSessionSmokeOptions(&RepeatEntry, false, true, 2, vdtrace::FlowHitPolicy::FirstSeen, 0));
            const TraceCaseResult repeat_every_case = RunTraceCase(
                L"VDTraceSessionSmokeRepeatEvery.log",
                MakeSessionSmokeOptions(&RepeatEntry, false, true, 2, vdtrace::FlowHitPolicy::EveryHit, 0));
            Require(CountLines(repeat_every_case.log_text) > CountLines(repeat_first_case.log_text), "every-hit trace did not record more lines");
        }

        if (ShouldRunCase(selection, "helper-edge-only"))
        {
            AnnounceSessionSmokeCase("helper-edge-only");
            const TraceCaseResult helper_edge_case = RunTraceCase(
                L"VDTraceSessionSmokeCrossHelperEdge.log",
                MakeSessionSmokeOptions(&CrossModuleHelperEntry, false, true, 4, vdtrace::FlowHitPolicy::EveryHit, 0));
            Require(Lowercase(helper_edge_case.log_text).find("vdtracetriggerwaithelper.dll") != std::string::npos, "edge-only helper trace did not record helper edge");
        }

        if (ShouldRunCase(selection, "helper-follow"))
        {
            AnnounceSessionSmokeCase("helper-follow");
            const TraceCaseResult helper_edge_case = RunTraceCase(
                L"VDTraceSessionSmokeCrossHelperEdgeBaseline.log",
                MakeSessionSmokeOptions(&CrossModuleHelperEntry, false, true, 4, vdtrace::FlowHitPolicy::EveryHit, 0));
            const TraceCaseResult helper_follow_case = RunTraceCase(
                L"VDTraceSessionSmokeCrossHelperFollow.log",
                MakeSessionSmokeOptions(&CrossModuleHelperEntry, true, true, 4, vdtrace::FlowHitPolicy::EveryHit, 0));
            Require(Lowercase(helper_follow_case.log_text).find("vdtracetriggerwaithelper.dll") != std::string::npos, "follow helper trace did not enter helper dll");
            Require(helper_follow_case.log_text.find("[fn]") != std::string::npos, "follow helper trace did not emit function preview");
            Require(
                ParseStateCounter(helper_follow_case.state_text, L"steps=") > ParseStateCounter(helper_edge_case.state_text, L"steps=") + 50,
                "edge-only helper trace still burned too many external steps");
        }

        if (ShouldRunCase(selection, "outside-depth-filter"))
        {
            AnnounceSessionSmokeCase("outside-depth-filter");
            const TraceCaseResult helper_same_limit_case = RunTraceCase(
                L"VDTraceSessionSmokeCrossHelperSame.log",
                MakeSessionSmokeOptions(&CrossModuleHelperEntry, true, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0));
            Require(Lowercase(helper_same_limit_case.log_text).find("outside!seq=") == std::string::npos, "same-level outside trace still entered helper module internals");
            const TraceCaseResult outside_depth_case = RunTraceCase(
                L"VDTraceSessionSmokeOutsideDepthFilter.log",
                config.outside_depth_options);
            Require(Lowercase(outside_depth_case.log_text).find("outside!seq=") != std::string::npos, "outside-depth filter did not follow helper module internals");
        }

        if (ShouldRunCase(selection, "outside-tf-filter"))
        {
            AnnounceSessionSmokeCase("outside-tf-filter");
            const TraceCaseResult outside_tf_case = RunTraceCase(
                L"VDTraceSessionSmokeOutsideTfFilter.log",
                config.outside_tf_options);
            Require(Lowercase(outside_tf_case.log_text).find("kind=other") != std::string::npos, "outside TF filter did not emit ordinary helper instructions");
        }

        if (ShouldRunCase(selection, "module-depth-filter"))
        {
            AnnounceSessionSmokeCase("module-depth-filter");
            const TraceCaseResult module_depth_case = RunTraceCase(
                L"VDTraceSessionSmokeModuleDepthFilter.log",
                config.module_depth_options);
            Require(Lowercase(module_depth_case.log_text).find("outside!seq=") != std::string::npos, "module-depth filter did not reopen the targeted helper module");
        }

        if (ShouldRunCase(selection, "module-tf-filter"))
        {
            AnnounceSessionSmokeCase("module-tf-filter");
            const TraceCaseResult module_tf_case = RunTraceCase(
                L"VDTraceSessionSmokeModuleTfFilter.log",
                config.module_tf_options);
            Require(Lowercase(module_tf_case.log_text).find("kind=other") != std::string::npos, "module TF filter did not emit ordinary helper instructions");
        }

        if (ShouldRunCase(selection, "anonymous-depth-filter"))
        {
            AnnounceSessionSmokeCase("anonymous-depth-filter");
            const TraceCaseResult anonymous_same_limit_case = RunTraceCase(
                L"VDTraceSessionSmokeAnonymousSame.log",
                MakeSessionSmokeOptions(&AnonymousExecEntry, false, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0));
            Require(Lowercase(anonymous_same_limit_case.log_text).find("outside!seq=") == std::string::npos, "same-level anonymous trace still entered anonymous code");
            const TraceCaseResult anonymous_depth_case = RunTraceCase(
                L"VDTraceSessionSmokeAnonymousDepthFilter.log",
                config.anonymous_depth_options);
            Require(Lowercase(anonymous_depth_case.log_text).find("anon-exec@") != std::string::npos, "anonymous-depth filter did not mark the anonymous executable range");
            Require(Lowercase(anonymous_depth_case.log_text).find("outside!seq=") != std::string::npos, "anonymous-depth filter did not follow anonymous executable internals");
            Require(Lowercase(anonymous_depth_case.log_text).find("[fn] anon-exec@") != std::string::npos, "anonymous-depth filter did not emit anonymous executable function preview");
            Require(Lowercase(anonymous_depth_case.log_text).find("minimal=1") == std::string::npos, "anonymous-depth filter still fell back to minimal outside logging");
            Require(anonymous_depth_case.log_text.find("[HeapPeek]->") != std::string::npos, "anonymous-depth filter did not emit heap peek for anonymous heap indirection");
            Require(Lowercase(anonymous_depth_case.log_text).find("]=[0x") != std::string::npos, "anonymous-depth filter did not emit heap peek resolved address");
        }

        if (ShouldRunCase(selection, "anonymous-tf-filter"))
        {
            AnnounceSessionSmokeCase("anonymous-tf-filter");
            const TraceCaseResult anonymous_tf_case = RunTraceCase(
                L"VDTraceSessionSmokeAnonymousTfFilter.log",
                config.anonymous_tf_options);
            Require(Lowercase(anonymous_tf_case.log_text).find("kind=other") != std::string::npos, "anonymous TF filter did not emit ordinary anonymous instructions");
            Require(Lowercase(anonymous_tf_case.log_text).find("[disasm] 0x") != std::string::npos, "anonymous TF filter did not emit disassembly for anonymous instructions");
        }

        if (ShouldRunCase(selection, "system-module"))
        {
            AnnounceSessionSmokeCase("system-module");
            const TraceCaseResult system_skip_case = RunTraceCase(
                L"VDTraceSessionSmokeSystemSkip.log",
                MakeSessionSmokeOptions(&CrossModuleSystemEntry, false, true, 1, vdtrace::FlowHitPolicy::EveryHit, 0));
            const std::string system_skip_log = Lowercase(system_skip_case.log_text);
            Require(
                (system_skip_log.find("kernel32") != std::string::npos || system_skip_log.find("kernelbase") != std::string::npos)
                    && system_skip_log.find("outside!seq=") == std::string::npos,
                "system-module trace did not stay at edge-only behavior");
        }

        if (ShouldRunCase(selection, "all-events"))
        {
            AnnounceSessionSmokeCase("all-events");
            const TraceCaseResult all_events_case = RunTraceCase(
                L"VDTraceSessionSmokeAllEvents.log",
                MakeSessionSmokeOptions(&AllEventsEntry, false, false, 1, vdtrace::FlowHitPolicy::EveryHit, 128));
            Require(Lowercase(all_events_case.log_text).find("kind=other") != std::string::npos, "all-events trace did not emit ordinary instructions");
        }
    }
}
