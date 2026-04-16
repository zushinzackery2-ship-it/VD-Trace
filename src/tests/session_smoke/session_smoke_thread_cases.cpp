#include "session_smoke_cli.h"

namespace session_smoke
{
    void RunSessionSmokeThreadCases(const SessionSmokeSelection &selection, const SessionSmokeConfig &config)
    {
        if (ShouldRunCase(selection, "auto-thread-capture"))
        {
            AnnounceSessionSmokeCase("auto-thread-capture");
            const TraceCaseResult auto_thread_case = RunTraceCase(
                L"VDTraceSessionSmokeAutoThread.log",
                MakeSessionSmokeOptions(&SameLevelEntry, false, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0, true, true));
            Require(ParseStateCounter(auto_thread_case.state_text, L"active_thread=") == auto_thread_case.worker_thread_id, "auto-thread capture did not switch to the triggered worker thread");
            Require(ParseStateCounter(auto_thread_case.state_text, L"capture_hits=") >= 1, "auto-thread capture did not record trigger hit count");
            Require(ParseStateCounter(auto_thread_case.state_text, L"capture_last=") == auto_thread_case.worker_thread_id, "auto-thread capture did not record the last trigger thread");
        }

        if (ShouldRunCase(selection, "auto-thread-capture-delayed"))
        {
            AnnounceSessionSmokeCase("auto-thread-capture-delayed");
            const TraceCaseResult delayed_auto_thread_case = RunDelayedAutoThreadCaptureCase(
                L"VDTraceSessionSmokeAutoThreadDelayed.log",
                &SameLevelEntry);
            Require(ParseStateCounter(delayed_auto_thread_case.state_text, L"active_thread=") == delayed_auto_thread_case.worker_thread_id, "delayed auto-thread capture did not switch to the late-created worker thread");
            Require(ParseStateCounter(delayed_auto_thread_case.state_text, L"capture_hits=") >= 1, "delayed auto-thread capture did not record trigger hit count");
            Require(ParseStateCounter(delayed_auto_thread_case.state_text, L"capture_last=") == delayed_auto_thread_case.worker_thread_id, "delayed auto-thread capture did not record the late-created trigger thread");
            Require(!delayed_auto_thread_case.log_text.empty(), "delayed auto-thread capture produced no log output");
        }

        if (ShouldRunCase(selection, "auto-thread-delayed-all-events"))
        {
            AnnounceSessionSmokeCase("auto-thread-delayed-all-events");
            const TraceCaseResult delayed_all_events_case = RunDelayedTraceCase(
                L"VDTraceSessionSmokeAutoThreadAllEvents.log",
                config.delayed_all_events_options);
            Require(ParseStateCounter(delayed_all_events_case.state_text, L"active_thread=") == delayed_all_events_case.worker_thread_id, "delayed all-events auto-thread trace did not switch to the late-created worker thread");
            Require(Lowercase(delayed_all_events_case.log_text).find("kind=other") != std::string::npos, "delayed all-events auto-thread trace did not emit ordinary instructions");
        }

        if (ShouldRunCase(selection, "main-thread-competition"))
        {
            AnnounceSessionSmokeCase("main-thread-competition");
            const TraceRunOptions main_thread_competition_options = MakeSessionSmokeOptions(&SameLevelEntry, false, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0, true, true);
            const TraceCaseResult main_thread_competition_case = RunMainThreadCompetitionCase(
                L"VDTraceSessionSmokeMainThreadCompetition.log",
                main_thread_competition_options);
            Require(ParseStateCounter(main_thread_competition_case.state_text, L"active_thread=") == main_thread_competition_case.main_thread_id, "main-thread competition trace did not capture the main thread when it was allowed");
            Require(Lowercase(main_thread_competition_case.log_text).find("[tid=" + std::to_string(main_thread_competition_case.main_thread_id) + "]") != std::string::npos, "main-thread competition log missed the main thread");
        }

        if (ShouldRunCase(selection, "block-main-thread"))
        {
            AnnounceSessionSmokeCase("block-main-thread");
            const TraceRunOptions block_main_thread_options = MakeSessionSmokeOptions(&SameLevelEntry, false, true, 0, vdtrace::FlowHitPolicy::EveryHit, 0, true, true, false, false, true);
            const TraceCaseResult block_main_thread_case = RunMainThreadCompetitionCase(
                L"VDTraceSessionSmokeBlockMainThread.log",
                block_main_thread_options);
            Require(ParseStateCounter(block_main_thread_case.state_text, L"active_thread=") == block_main_thread_case.worker_thread_id, "block-main-thread trace did not switch to the worker thread");
            Require(block_main_thread_case.state_text.find(L"block_main=1") != std::wstring::npos, "block-main-thread state did not expose the new option");
            Require(Lowercase(block_main_thread_case.log_text).find("[tid=" + std::to_string(block_main_thread_case.main_thread_id) + "]") == std::string::npos, "block-main-thread log still recorded the main thread");
        }

        if (ShouldRunCase(selection, "unity-worker-precreated"))
        {
            AnnounceSessionSmokeCase("unity-worker-precreated");
            const TraceCaseResult unity_worker_case = RunUnityWorkerCaptureCase(L"VDTraceSessionSmokeUnityWorker.log");
            Require(ParseStateCounter(unity_worker_case.state_text, L"active_thread=") == unity_worker_case.worker_thread_id, "unity worker capture did not lock onto the precreated worker thread");
            Require(ParseStateCounter(unity_worker_case.state_text, L"capture_hits=") >= 1, "unity worker capture did not record trigger hit count");
            Require(ParseStateCounter(unity_worker_case.state_text, L"capture_last=") == unity_worker_case.worker_thread_id, "unity worker capture did not record the precreated worker thread");
            Require(unity_worker_case.auto_stopped, "unity worker capture did not root-stop on the precreated worker path");
        }

        if (ShouldRunCase(selection, "unity-worker-follow-helper"))
        {
            AnnounceSessionSmokeCase("unity-worker-follow-helper");
            const TraceCaseResult unity_follow_case = RunUnityWorkerTraceCase(
                L"VDTraceSessionSmokeUnityWorkerFollow.log",
                config.unity_follow_options);
            Require(ParseStateCounter(unity_follow_case.state_text, L"active_thread=") == unity_follow_case.worker_thread_id, "unity worker follow trace did not stay on the precreated worker thread");
            Require(Lowercase(unity_follow_case.log_text).find("vdtracetriggerwaithelper.dll") != std::string::npos, "unity worker follow trace did not enter helper dll");
        }

        if (ShouldRunCase(selection, "trigger-queue-rotation"))
        {
            AnnounceSessionSmokeCase("trigger-queue-rotation");
            const TraceRunOptions queue_rotation_options = MakeSessionSmokeOptions(&UnityWorkerAssetEntry, false, true, 4, vdtrace::FlowHitPolicy::FirstSeen, 0, true, true, true);
            const TraceCaseResult queue_rotation_case = RunUnityWorkerQueueRotationCase(
                L"VDTraceSessionSmokeQueueRotation.log",
                queue_rotation_options);
            Require(queue_rotation_case.state_text.find(L"focus=queue") != std::wstring::npos, "queue rotation state did not expose queue focus");
            Require(ParseStateCounter(queue_rotation_case.state_text, L"capture_hits=") >= 2, "queue rotation did not record multiple trigger hits");
            Require(Lowercase(queue_rotation_case.log_text).find("[tid=" + std::to_string(queue_rotation_case.worker_thread_id) + "]") != std::string::npos, "queue rotation log missed the first worker thread");
            Require(Lowercase(queue_rotation_case.log_text).find("[tid=" + std::to_string(queue_rotation_case.second_worker_thread_id) + "]") != std::string::npos, "queue rotation log missed the second worker thread");
        }

        if (ShouldRunCase(selection, "probe-queue-rotation"))
        {
            AnnounceSessionSmokeCase("probe-queue-rotation");
            const TraceCaseResult probe_queue_case = RunUnityWorkerQueueRotationCase(
                L"VDTraceSessionSmokeProbeQueue.log",
                config.probe_queue_options);
            Require(probe_queue_case.state_text.find(L"focus=queue") != std::wstring::npos, "probe queue rotation state did not expose queue focus");
            Require(ParseStateCounter(probe_queue_case.state_text, L"capture_hits=") >= 2, "probe queue rotation did not record multiple trigger hits");
            Require(Lowercase(probe_queue_case.log_text).find("kind=probe") != std::string::npos, "probe queue rotation did not emit probe events");
            Require(Lowercase(probe_queue_case.log_text).find("[tid=" + std::to_string(probe_queue_case.worker_thread_id) + "]") != std::string::npos, "probe queue rotation log missed the first worker thread");
            Require(Lowercase(probe_queue_case.log_text).find("[tid=" + std::to_string(probe_queue_case.second_worker_thread_id) + "]") != std::string::npos, "probe queue rotation log missed the second worker thread");
        }

        if (ShouldRunCase(selection, StabilityGroupCaseName()))
        {
            AnnounceSessionSmokeCase(StabilityGroupCaseName());
            for (uint32_t round = 1; round <= selection.rounds; ++round)
            {
                if (ShouldRunStabilityCase(selection, "delayed-auto-thread"))
                {
                    const TraceCaseResult delayed_round = RunDelayedTraceCase(
                        RoundLog(L"VDTraceSessionSmokeAutoThreadRound", static_cast<int>(round)),
                        config.delayed_all_events_options,
                        40);
                    Require(ParseStateCounter(delayed_round.state_text, L"active_thread=") == delayed_round.worker_thread_id, "stability round delayed auto-thread capture lost the target thread");
                }

                if (ShouldRunStabilityCase(selection, "unity-worker"))
                {
                    const TraceCaseResult unity_round = RunUnityWorkerCaptureCase(RoundLog(L"VDTraceSessionSmokeUnityWorkerRound", static_cast<int>(round)));
                    Require(ParseStateCounter(unity_round.state_text, L"active_thread=") == unity_round.worker_thread_id, "stability round unity worker capture lost the worker thread");
                }

                if (ShouldRunStabilityCase(selection, "unity-worker-follow"))
                {
                    const TraceCaseResult unity_follow_round = RunUnityWorkerTraceCase(
                        RoundLog(L"VDTraceSessionSmokeUnityWorkerFollowRound", static_cast<int>(round)),
                        config.unity_follow_options);
                    Require(ParseStateCounter(unity_follow_round.state_text, L"active_thread=") == unity_follow_round.worker_thread_id, "stability round unity worker follow trace lost the worker thread");
                }

                if (ShouldRunStabilityCase(selection, "probe"))
                {
                    const TraceCaseResult probe_round = RunTraceCase(
                        RoundLog(L"VDTraceSessionSmokeProbeRound", static_cast<int>(round)),
                        config.probe_options);
                    Require(Lowercase(probe_round.log_text).find("kind=probe") != std::string::npos, "stability round probe trace did not emit probe event");
                }

                if (ShouldRunStabilityCase(selection, "probe-queue"))
                {
                    const TraceCaseResult probe_queue_round = RunUnityWorkerQueueRotationCase(
                        RoundLog(L"VDTraceSessionSmokeProbeQueueRound", static_cast<int>(round)),
                        config.probe_queue_options);
                    Require(ParseStateCounter(probe_queue_round.state_text, L"capture_hits=") >= 2, "stability round probe queue rotation did not record multiple trigger hits");
                }

                if (ShouldRunStabilityCase(selection, "outside-depth-filter"))
                {
                    const TraceCaseResult outside_depth_round = RunTraceCase(
                        RoundLog(L"VDTraceSessionSmokeOutsideDepthRound", static_cast<int>(round)),
                        config.outside_depth_options);
                    Require(Lowercase(outside_depth_round.log_text).find("outside!seq=") != std::string::npos, "stability round outside-depth filter lost helper-module follow");
                }

                if (ShouldRunStabilityCase(selection, "outside-tf-filter"))
                {
                    const TraceCaseResult outside_tf_round = RunTraceCase(
                        RoundLog(L"VDTraceSessionSmokeOutsideTfRound", static_cast<int>(round)),
                        config.outside_tf_options);
                    Require(Lowercase(outside_tf_round.log_text).find("kind=other") != std::string::npos, "stability round outside TF filter lost ordinary helper instructions");
                }

                if (ShouldRunStabilityCase(selection, "module-depth-filter"))
                {
                    const TraceCaseResult module_depth_round = RunTraceCase(
                        RoundLog(L"VDTraceSessionSmokeModuleDepthRound", static_cast<int>(round)),
                        config.module_depth_options);
                    Require(Lowercase(module_depth_round.log_text).find("outside!seq=") != std::string::npos, "stability round module-depth filter lost module-specific follow");
                }

                if (ShouldRunStabilityCase(selection, "module-tf-filter"))
                {
                    const TraceCaseResult module_tf_round = RunTraceCase(
                        RoundLog(L"VDTraceSessionSmokeModuleTfRound", static_cast<int>(round)),
                        config.module_tf_options);
                    Require(Lowercase(module_tf_round.log_text).find("kind=other") != std::string::npos, "stability round module TF filter lost ordinary helper instructions");
                }

                if (ShouldRunStabilityCase(selection, "anonymous-depth-filter"))
                {
                    const TraceCaseResult anonymous_depth_round = RunTraceCase(
                        RoundLog(L"VDTraceSessionSmokeAnonymousDepthRound", static_cast<int>(round)),
                        config.anonymous_depth_options);
                    Require(Lowercase(anonymous_depth_round.log_text).find("outside!seq=") != std::string::npos, "stability round anonymous-depth filter lost anonymous follow");
                }

                if (ShouldRunStabilityCase(selection, "anonymous-tf-filter"))
                {
                    const TraceCaseResult anonymous_tf_round = RunTraceCase(
                        RoundLog(L"VDTraceSessionSmokeAnonymousTfRound", static_cast<int>(round)),
                        config.anonymous_tf_options);
                    Require(Lowercase(anonymous_tf_round.log_text).find("kind=other") != std::string::npos, "stability round anonymous TF filter lost ordinary anonymous instructions");
                }
            }
        }
    }
}
