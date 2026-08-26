cmake_minimum_required(VERSION 3.24)
project(VDTrace LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/release")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/release")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/release")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin/release")

if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc /MP /utf-8 /Zc:__cplusplus)
    add_compile_definitions(_UNICODE UNICODE NOMINMAX WIN32_LEAN_AND_MEAN ZYCORE_STATIC_BUILD ZYDIS_STATIC_BUILD)
endif()

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/third_party"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/zydis"
)

set(VDTRACE_CORE_SOURCES
    src/core/api/VDTraceC.cpp
    src/core/api/VDTraceCSupport.cpp
    src/core/api/VDTraceDllMain.cpp
    src/core/api/VDTraceIpc.cpp
    src/core/async/VDTraceAsyncHandoff.cpp
    src/core/async/VDTraceAsyncSupport.cpp
    src/core/async/VDTraceAsyncSupportUtil.cpp
    src/core/async/VDTraceAsyncThreadState.cpp
    src/core/decoder/VDTraceDecoder.cpp
    src/core/decoder/VDTraceDecoderSupport.cpp
    src/core/decoder/VDTraceInstruction.cpp
    src/core/depth_filter/VDTraceDepthFilter.cpp
    src/core/depth_filter/VDTraceDepthFilterParse.cpp
    src/core/depth_filter/VDTraceDepthFilterResolve.cpp
    src/core/extender/VDTraceExtender.cpp
    src/core/extender/VDTraceExtenderAnalyze.cpp
    src/core/extender/VDTraceExtenderAnalyzeDecode.cpp
    src/core/extender/VDTraceExtenderAnalyzeEffects.cpp
    src/core/extender/VDTraceExtenderAnalyzeMemory.cpp
    src/core/extender/VDTraceExtenderAnalyzeSim.cpp
    src/core/extender/VDTraceExtenderProcess.cpp
    src/core/hardware/VDTraceHardware.cpp
    src/core/hardware/VDTraceHardwareContext.cpp
    src/core/hardware/VDTraceHardwareException.cpp
    src/core/hardware/VDTraceHardwareFallback.cpp
    src/core/hardware/VDTraceHardwareObserve.cpp
    src/core/hardware/VDTraceHardwareSupport.cpp
    src/core/hardware/VDTraceHardwareTransition.cpp
    src/core/hardware/VDTraceHardwareTransitionSupport.cpp
    src/core/heap_peek/VDTraceHeapPeek.cpp
    src/core/heap_peek/VDTraceHeapPeekSupport.cpp
    src/core/heap_peek/VDTraceHeapPeekSupportDetail.cpp
    src/core/names/VDTraceNames.cpp
    src/core/observer/VDTraceObserver.cpp
    src/core/preview/VDTraceFunctionPreview.cpp
    src/core/preview/VDTraceFunctionPreviewInline.cpp
    src/core/preview/VDTraceFunctionPreviewSupport.cpp
    src/core/probe/VDTraceProbe.cpp
    src/core/probe/VDTraceProbeObserver.cpp
    src/core/probe/VDTraceProbeSupport.cpp
    src/core/recorder/VDTraceFileSink.cpp
    src/core/recorder/VDTraceFileSinkFormat.cpp
    src/core/recorder/VDTraceFileSinkFormatBlocks.cpp
    src/core/recorder/VDTraceFileSinkFormatLabels.cpp
    src/core/recorder/VDTraceFileSinkFormatSupport.cpp
    src/core/recorder/VDTraceFileSinkWorker.cpp
    src/core/recorder/VDTraceFileSinkWorkerProcess.cpp
    src/core/recorder/VDTraceFileSinkWriter.cpp
    src/core/runtime/VDTraceExecutionMode.cpp
    src/core/runtime/VDTraceRuntime.cpp
    src/core/runtime/VDTraceRuntimeConfig.cpp
    src/core/runtime/VDTraceRuntimeConfigure.cpp
    src/core/runtime/VDTraceRuntimeState.cpp
    src/core/runtime/VDTraceRuntimeSupport.cpp
    src/core/runtime/VDTraceSession.cpp
    src/core/sampling/VDTraceEnhancedSampling.cpp
    src/core/sampling/VDTraceEnhancedSamplingFormat.cpp
    src/core/static_refs/VDTraceStaticRefs.cpp
    src/core/static_refs/VDTraceStaticRefsExport.cpp
    src/core/static_refs/VDTraceStaticRefsFormat.cpp
    src/core/static_refs/VDTraceStaticRefsGuess.cpp
    src/core/static_refs/VDTraceStaticRefsJson.cpp
    src/core/static_refs/VDTraceStaticRefsSupport.cpp
    src/core/support/VDTraceValuePreview.cpp
    src/core/threading/VDTraceThreadContext.cpp
    src/core/threading/VDTraceThreadContextSupport.cpp
    src/core/threading/VDTraceThreadDiscovery.cpp
    src/core/trigger/VDTraceTriggerCapture.cpp
    src/core/trigger/VDTraceTriggerCaptureSupport.cpp
    src/core/trigger/VDTraceTriggerWait.cpp
    src/third_party/zydis/Zydis.c
)

set(VDTRACE_AGENT_SOURCES
    src/agent/VDTraceAgentDump.cpp
    src/agent/VDTraceAgentDumpSupport.cpp
    src/agent/VDTraceAgentExports.cpp
    src/agent/VDTraceAgentIpc.cpp
    src/agent/VDTraceAgentIpcSupport.cpp
    src/agent/VDTraceAgentMemory.cpp
    src/agent/VDTraceAgentMemoryFormat.cpp
    src/agent/VDTraceAgentMemorySupport.cpp
    src/agent/VDTraceAgentState.cpp
    src/agent/VDTraceAgentStateSupport.cpp
    src/agent/VDTracePeLayout.cpp
)

set(VDTRACE_AUTOSTART_SOURCES
    src/autostart/VDTraceAutoStartConfig.cpp
    src/autostart/VDTraceAutoStartConfigText.cpp
    src/autostart/VDTraceAutoStartHelper.cpp
    src/autostart/VDTraceAutoStartHelperSupport.cpp
    src/autostart/VDTraceAutoStartWait.cpp
    src/control/ipc_client/VDTraceControlSupport.cpp
    src/control/ipc_client/VDTraceControlSupportUtil.cpp
)

set(VDTRACE_CONTROL_SOURCES
    src/control/ipc_client/VDTraceControlSupport.cpp
    src/control/ipc_client/VDTraceControlSupportUtil.cpp
)

set(VDTRACE_LOADER_CONTROL_SOURCES
    src/control/loader_session/VDTraceLoaderControlSupport.cpp
)

set(VDTRACE_CTL_SOURCES
    src/tools/vdtrace_ctl/vdtrace_ctl.cpp
    src/tools/vdtrace_ctl/vdtrace_ctl_command.cpp
    src/tools/vdtrace_ctl/vdtrace_ctl_parse.cpp
    src/control/injection/VDTraceControlSupportInject.cpp
)

set(VDTRACE_AUTOSTART_CLI_SOURCES
    src/tools/vdtrace_autostart/vdtrace_autostart.cpp
    src/tools/vdtrace_autostart/vdtrace_autostart_install.cpp
    src/tools/vdtrace_autostart/vdtrace_autostart_io.cpp
    src/autostart/VDTraceAutoStartConfig.cpp
    src/autostart/VDTraceAutoStartConfigText.cpp
)

set(VDTRACE_EXAMPLE_SOURCES
    src/tools/examples/vdtrace_example.cpp
)

set(VDTRACE_SESSION_SMOKE_SOURCES
    src/tests/session_smoke/session_smoke_cli.cpp
    src/tests/session_smoke/session_smoke_config.cpp
    src/tests/session_smoke/session_smoke_runtime.cpp
    src/tests/session_smoke/session_smoke_runtime_entries.cpp
    src/tests/session_smoke/session_smoke_thread_cases.cpp
    src/tests/session_smoke/session_smoke_trace.cpp
    src/tests/session_smoke/session_smoke_trace_cases_basic.cpp
    src/tests/session_smoke/session_smoke_trace_cases_features.cpp
    src/tests/session_smoke/session_smoke_trace_cases_recorder.cpp
    src/tests/session_smoke/session_smoke_trace_helpers.cpp
    src/tests/session_smoke/session_smoke_trace_workers.cpp
    src/tests/session_smoke/vdtrace_session_smoke_test.cpp
)

set(VDTRACE_DECRYPT_SMOKE_TEST_SOURCES
    src/tests/decrypt_smoke/vdtrace_decrypt_smoke_support.cpp
    src/tests/decrypt_smoke/vdtrace_decrypt_smoke_test.cpp
)

set(VDTRACE_DECRYPT_HELPER_SOURCES
    src/tests/decrypt_smoke/VDTraceDecryptSmokeHelper.cpp
    src/tests/decrypt_smoke/VDTraceDecryptSmokeHelperRuntime.cpp
)

set(VDTRACE_TRIGGER_HELPER_SOURCES
    src/tests/trigger_wait/VDTraceTriggerWaitHelper.cpp
)

set(VDTRACE_TRIGGER_WAIT_SOURCES
    src/tests/trigger_wait/vdtrace_trigger_wait_test.cpp
)

set(VDTRACE_ROOTSTOP_SOURCES
    src/tests/trigger_wait/vdtrace_rootstop_test.cpp
)

set(VDTRACE_STOP_RECOVERY_SOURCES
    src/tests/trigger_wait/vdtrace_stop_recovery_test.cpp
)

set(VDTRACE_ASYNC_SMOKE_SOURCES
    src/tests/async_smoke/vdtrace_async_handoff_smoke_test.cpp
)

set(VDTRACE_AGENT_SMOKE_SOURCES
    src/tests/agent_smoke/vdtrace_agent_smoke_test.cpp
)

set(VDTRACE_SMOKE_SUITE_SOURCES
    src/tests/smoke_suite/vdtrace_smoke_suite_test.cpp
)


function(vdtrace_apply_common_settings target)
    target_precompile_headers(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.h>)
    target_link_libraries(${target} PRIVATE psapi)
endfunction()

add_library(VDTraceStatic STATIC ${VDTRACE_CORE_SOURCES})
vdtrace_apply_common_settings(VDTraceStatic)

add_library(VDTrace SHARED ${VDTRACE_CORE_SOURCES} src/VDTrace.def)
vdtrace_apply_common_settings(VDTrace)

add_library(VDTraceAgent SHARED ${VDTRACE_AGENT_SOURCES})
vdtrace_apply_common_settings(VDTraceAgent)
target_link_libraries(VDTraceAgent PRIVATE VDTraceStatic)

add_library(VDTraceAutoStart SHARED ${VDTRACE_AUTOSTART_SOURCES})
vdtrace_apply_common_settings(VDTraceAutoStart)
target_link_libraries(VDTraceAutoStart PRIVATE VDTraceStatic)

add_library(VDTraceTriggerWaitHelper SHARED ${VDTRACE_TRIGGER_HELPER_SOURCES})
vdtrace_apply_common_settings(VDTraceTriggerWaitHelper)

add_library(VDTraceDecryptSmokeHelper SHARED ${VDTRACE_DECRYPT_HELPER_SOURCES})
vdtrace_apply_common_settings(VDTraceDecryptSmokeHelper)

add_library(VDTraceControlSupport STATIC ${VDTRACE_CONTROL_SOURCES})
vdtrace_apply_common_settings(VDTraceControlSupport)

target_link_libraries(VDTraceControlSupport PRIVATE VDTraceStatic)

add_library(VDTraceLoaderControlSupport STATIC ${VDTRACE_LOADER_CONTROL_SOURCES})
vdtrace_apply_common_settings(VDTraceLoaderControlSupport)

add_executable(vdtrace_ctl ${VDTRACE_CTL_SOURCES})
vdtrace_apply_common_settings(vdtrace_ctl)
target_link_libraries(vdtrace_ctl PRIVATE VDTraceControlSupport VDTraceStatic)

add_executable(vdtrace_autostart ${VDTRACE_AUTOSTART_CLI_SOURCES})
vdtrace_apply_common_settings(vdtrace_autostart)
target_link_libraries(vdtrace_autostart PRIVATE VDTraceControlSupport VDTraceStatic)

add_executable(vdtrace_example ${VDTRACE_EXAMPLE_SOURCES})
vdtrace_apply_common_settings(vdtrace_example)
target_link_libraries(vdtrace_example PRIVATE VDTraceStatic)

add_executable(vdtrace_session_smoke_test ${VDTRACE_SESSION_SMOKE_SOURCES})
vdtrace_apply_common_settings(vdtrace_session_smoke_test)
target_link_libraries(vdtrace_session_smoke_test PRIVATE VDTraceStatic)

add_executable(vdtrace_async_handoff_smoke_test ${VDTRACE_ASYNC_SMOKE_SOURCES})
vdtrace_apply_common_settings(vdtrace_async_handoff_smoke_test)
target_link_libraries(vdtrace_async_handoff_smoke_test PRIVATE VDTraceStatic)

add_executable(vdtrace_agent_smoke_test ${VDTRACE_AGENT_SMOKE_SOURCES})
vdtrace_apply_common_settings(vdtrace_agent_smoke_test)
target_link_libraries(vdtrace_agent_smoke_test PRIVATE VDTraceControlSupport VDTraceStatic)

add_executable(vdtrace_trigger_wait_test ${VDTRACE_TRIGGER_WAIT_SOURCES})
vdtrace_apply_common_settings(vdtrace_trigger_wait_test)
target_link_libraries(vdtrace_trigger_wait_test PRIVATE VDTraceStatic)

add_executable(vdtrace_rootstop_test ${VDTRACE_ROOTSTOP_SOURCES})
vdtrace_apply_common_settings(vdtrace_rootstop_test)
target_link_libraries(vdtrace_rootstop_test PRIVATE VDTraceStatic)

add_executable(vdtrace_stop_recovery_test ${VDTRACE_STOP_RECOVERY_SOURCES})
vdtrace_apply_common_settings(vdtrace_stop_recovery_test)
target_link_libraries(vdtrace_stop_recovery_test PRIVATE VDTraceStatic)

add_executable(vdtrace_decrypt_smoke_test ${VDTRACE_DECRYPT_SMOKE_TEST_SOURCES})
vdtrace_apply_common_settings(vdtrace_decrypt_smoke_test)
target_link_libraries(vdtrace_decrypt_smoke_test PRIVATE VDTraceStatic)

add_executable(vdtrace_smoke_suite_test ${VDTRACE_SMOKE_SUITE_SOURCES})
vdtrace_apply_common_settings(vdtrace_smoke_suite_test)

add_dependencies(
    vdtrace_smoke_suite_test
    vdtrace_async_handoff_smoke_test
    vdtrace_agent_smoke_test
    vdtrace_trigger_wait_test
    vdtrace_rootstop_test
    vdtrace_stop_recovery_test
    vdtrace_decrypt_smoke_test
    VDTraceAgent
    VDTraceTriggerWaitHelper
    VDTraceDecryptSmokeHelper
)
