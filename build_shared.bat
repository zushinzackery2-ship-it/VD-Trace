@echo off
setlocal

set SCRIPT_ROOT=%~dp0
if "%SCRIPT_ROOT%"=="" set SCRIPT_ROOT=.

set BUILD_MODE=%~1
if "%BUILD_MODE%"=="" set BUILD_MODE=full

call "D:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1

set BUILD_TEMP=%SCRIPT_ROOT%obj\tmp
if not exist "%BUILD_TEMP%" mkdir "%BUILD_TEMP%"
set TEMP=%BUILD_TEMP%
set TMP=%BUILD_TEMP%

set DOTNET_CLI_HOME=%CD%\.dotnet_cli_home
set DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
set DOTNET_NOLOGO=1
if not exist "%DOTNET_CLI_HOME%" mkdir "%DOTNET_CLI_HOME%"

if /I "%BUILD_MODE%"=="full" (
    set BUILD_FULL=1
) else (
    set BUILD_FULL=0
)

if "%BUILD_FULL%"=="1" (
    set SYSTEM_WINHTTP=%SystemRoot%\System32\winhttp.dll
)

for %%D in (
    "bin\release"
    "obj\release\vdtrace"
    "obj\release\vdtrace_example"
    "obj\release\vdtrace_agent"
    "obj\release\vdtrace_smoke"
    "obj\release\vdtrace_async_smoke"
    "obj\release\vdtrace_decrypt_smoke"
    "obj\release\vdtrace_session_smoke"
    "obj\release\vdtrace_smoke_suite"
    "obj\release\vdtrace_trigger_wait"
) do (
    if not exist %%~D mkdir %%~D
)

if "%BUILD_FULL%"=="1" (
    for %%D in (
        "obj\release\vdtrace_ctl"
        "obj\release\vdtrace_autostart"
        "obj\release\vdtrace_early_loader"
        "obj\release\vdtrace_loader"
    ) do (
        if not exist %%~D mkdir %%~D
    )
)

set CORE_CPP=^
src\VDTraceSession.cpp ^
src\VDTraceRuntime.cpp ^
src\VDTraceRuntimeSupport.cpp ^
src\VDTraceRuntimeConfig.cpp ^
src\VDTraceRuntimeConfigure.cpp ^
src\VDTraceRuntimeState.cpp ^
src\VDTraceAsyncSupport.cpp ^
src\VDTraceAsyncSupportUtil.cpp ^
src\VDTraceAsyncHandoff.cpp ^
src\VDTraceAsyncThreadState.cpp ^
src\VDTraceTriggerCapture.cpp ^
src\VDTraceTriggerCaptureSupport.cpp ^
src\VDTraceTriggerWait.cpp ^
src\VDTraceThreadDiscovery.cpp ^
src\VDTraceThreadContext.cpp ^
src\VDTraceThreadContextSupport.cpp ^
src\VDTraceDepthFilter.cpp ^
src\VDTraceDepthFilterResolve.cpp ^
src\VDTraceExecutionMode.cpp ^
src\VDTraceDecoder.cpp ^
src\VDTraceDecoderSupport.cpp ^
src\VDTraceHardware.cpp ^
src\VDTraceHardwareException.cpp ^
src\VDTraceHardwareContext.cpp ^
src\VDTraceHardwareFallback.cpp ^
src\VDTraceHardwareObserve.cpp ^
src\VDTraceHardwareSupport.cpp ^
src\VDTraceHardwareTransition.cpp ^
src\VDTraceHardwareTransitionSupport.cpp ^
src\VDTraceInstruction.cpp ^
src\VDTraceObserver.cpp ^
src\VDTraceProbe.cpp ^
src\VDTraceProbeSupport.cpp ^
src\VDTraceProbeObserver.cpp ^
src\VDTraceStaticRefs.cpp ^
src\VDTraceStaticRefsSupport.cpp ^
src\VDTraceStaticRefsGuess.cpp ^
src\VDTraceStaticRefsFormat.cpp ^
src\VDTraceStaticRefsExport.cpp ^
src\VDTraceStaticRefsJson.cpp ^
src\VDTraceEnhancedSampling.cpp ^
src\VDTraceEnhancedSamplingFormat.cpp ^
src\VDTraceHeapPeek.cpp ^
src\VDTraceHeapPeekSupport.cpp ^
src\VDTraceHeapPeekSupportDetail.cpp ^
src\VDTraceExtender.cpp ^
src\VDTraceExtenderProcess.cpp ^
src\VDTraceExtenderAnalyze.cpp ^
src\VDTraceExtenderAnalyzeSim.cpp ^
src\VDTraceExtenderAnalyzeDecode.cpp ^
src\VDTraceExtenderAnalyzeMemory.cpp ^
src\VDTraceExtenderAnalyzeEffects.cpp ^
src\VDTraceFunctionPreview.cpp ^
src\VDTraceFunctionPreviewSupport.cpp ^
src\VDTraceFunctionPreviewInline.cpp ^
src\VDTraceValuePreview.cpp ^
src\VDTraceC.cpp ^
src\VDTraceCSupport.cpp ^
src\VDTraceNames.cpp ^
src\VDTraceIpc.cpp ^
src\VDTraceFileSink.cpp ^
src\VDTraceFileSinkWriter.cpp ^
src\VDTraceFileSinkWorker.cpp ^
src\VDTraceFileSinkWorkerProcess.cpp ^
src\VDTraceFileSinkFormatSupport.cpp ^
src\VDTraceFileSinkFormatLabels.cpp ^
src\VDTraceFileSinkFormatBlocks.cpp ^
src\VDTraceFileSinkFormat.cpp

set CORE_OBJS=^
obj\release\vdtrace\pch.obj ^
obj\release\vdtrace\VDTraceSession.obj ^
obj\release\vdtrace\VDTraceRuntime.obj ^
obj\release\vdtrace\VDTraceRuntimeSupport.obj ^
obj\release\vdtrace\VDTraceRuntimeConfig.obj ^
obj\release\vdtrace\VDTraceRuntimeConfigure.obj ^
obj\release\vdtrace\VDTraceRuntimeState.obj ^
obj\release\vdtrace\Zydis.obj ^
obj\release\vdtrace\VDTraceAsyncSupport.obj ^
obj\release\vdtrace\VDTraceAsyncSupportUtil.obj ^
obj\release\vdtrace\VDTraceAsyncHandoff.obj ^
obj\release\vdtrace\VDTraceAsyncThreadState.obj ^
obj\release\vdtrace\VDTraceTriggerCapture.obj ^
obj\release\vdtrace\VDTraceTriggerCaptureSupport.obj ^
obj\release\vdtrace\VDTraceTriggerWait.obj ^
obj\release\vdtrace\VDTraceThreadDiscovery.obj ^
obj\release\vdtrace\VDTraceThreadContext.obj ^
obj\release\vdtrace\VDTraceThreadContextSupport.obj ^
obj\release\vdtrace\VDTraceDepthFilter.obj ^
obj\release\vdtrace\VDTraceDepthFilterResolve.obj ^
obj\release\vdtrace\VDTraceExecutionMode.obj ^
obj\release\vdtrace\VDTraceDecoder.obj ^
obj\release\vdtrace\VDTraceDecoderSupport.obj ^
obj\release\vdtrace\VDTraceHardware.obj ^
obj\release\vdtrace\VDTraceHardwareException.obj ^
obj\release\vdtrace\VDTraceHardwareContext.obj ^
obj\release\vdtrace\VDTraceHardwareFallback.obj ^
obj\release\vdtrace\VDTraceHardwareObserve.obj ^
obj\release\vdtrace\VDTraceHardwareSupport.obj ^
obj\release\vdtrace\VDTraceHardwareTransition.obj ^
obj\release\vdtrace\VDTraceHardwareTransitionSupport.obj ^
obj\release\vdtrace\VDTraceInstruction.obj ^
obj\release\vdtrace\VDTraceObserver.obj ^
obj\release\vdtrace\VDTraceProbe.obj ^
obj\release\vdtrace\VDTraceProbeSupport.obj ^
obj\release\vdtrace\VDTraceProbeObserver.obj ^
obj\release\vdtrace\VDTraceStaticRefs.obj ^
obj\release\vdtrace\VDTraceStaticRefsSupport.obj ^
obj\release\vdtrace\VDTraceStaticRefsGuess.obj ^
obj\release\vdtrace\VDTraceStaticRefsFormat.obj ^
obj\release\vdtrace\VDTraceStaticRefsExport.obj ^
obj\release\vdtrace\VDTraceStaticRefsJson.obj ^
obj\release\vdtrace\VDTraceEnhancedSampling.obj ^
obj\release\vdtrace\VDTraceEnhancedSamplingFormat.obj ^
obj\release\vdtrace\VDTraceHeapPeek.obj ^
obj\release\vdtrace\VDTraceHeapPeekSupport.obj ^
obj\release\vdtrace\VDTraceHeapPeekSupportDetail.obj ^
obj\release\vdtrace\VDTraceExtender.obj ^
obj\release\vdtrace\VDTraceExtenderProcess.obj ^
obj\release\vdtrace\VDTraceExtenderAnalyze.obj ^
obj\release\vdtrace\VDTraceExtenderAnalyzeSim.obj ^
obj\release\vdtrace\VDTraceExtenderAnalyzeDecode.obj ^
obj\release\vdtrace\VDTraceExtenderAnalyzeMemory.obj ^
obj\release\vdtrace\VDTraceExtenderAnalyzeEffects.obj ^
obj\release\vdtrace\VDTraceFunctionPreview.obj ^
obj\release\vdtrace\VDTraceFunctionPreviewSupport.obj ^
obj\release\vdtrace\VDTraceFunctionPreviewInline.obj ^
obj\release\vdtrace\VDTraceValuePreview.obj ^
obj\release\vdtrace\VDTraceC.obj ^
obj\release\vdtrace\VDTraceCSupport.obj ^
obj\release\vdtrace\VDTraceNames.obj ^
obj\release\vdtrace\VDTraceIpc.obj ^
obj\release\vdtrace\VDTraceFileSink.obj ^
obj\release\vdtrace\VDTraceFileSinkWriter.obj ^
obj\release\vdtrace\VDTraceFileSinkWorker.obj ^
obj\release\vdtrace\VDTraceFileSinkWorkerProcess.obj ^
obj\release\vdtrace\VDTraceFileSinkFormatSupport.obj ^
obj\release\vdtrace\VDTraceFileSinkFormatLabels.obj ^
obj\release\vdtrace\VDTraceFileSinkFormatBlocks.obj ^
obj\release\vdtrace\VDTraceFileSinkFormat.obj

set AGENT_CPP=^
src\agent\VDTracePeLayout.cpp ^
src\agent\VDTraceAgentDump.cpp ^
src\agent\VDTraceAgentMemory.cpp ^
src\agent\VDTraceAgentMemorySupport.cpp ^
src\agent\VDTraceAgentState.cpp ^
src\agent\VDTraceAgentStateSupport.cpp ^
src\agent\VDTraceAgentIpc.cpp ^
src\agent\VDTraceAgentExports.cpp

set AGENT_OBJS=^
obj\release\vdtrace_agent\VDTracePeLayout.obj ^
obj\release\vdtrace_agent\VDTraceAgentDump.obj ^
obj\release\vdtrace_agent\VDTraceAgentMemory.obj ^
obj\release\vdtrace_agent\VDTraceAgentMemorySupport.obj ^
obj\release\vdtrace_agent\VDTraceAgentState.obj ^
obj\release\vdtrace_agent\VDTraceAgentStateSupport.obj ^
obj\release\vdtrace_agent\VDTraceAgentIpc.obj ^
obj\release\vdtrace_agent\VDTraceAgentExports.obj

set SESSION_SMOKE_CPP=^
src\tests\session_smoke\session_smoke_runtime.cpp ^
src\tests\session_smoke\session_smoke_runtime_entries.cpp ^
src\tests\session_smoke\session_smoke_trace.cpp ^
src\tests\session_smoke\session_smoke_trace_helpers.cpp ^
src\tests\session_smoke\session_smoke_trace_workers.cpp ^
src\tests\session_smoke\session_smoke_cli.cpp ^
src\tests\session_smoke\session_smoke_config.cpp ^
src\tests\session_smoke\session_smoke_trace_cases_basic.cpp ^
src\tests\session_smoke\session_smoke_trace_cases_features.cpp ^
src\tests\session_smoke\session_smoke_trace_cases_recorder.cpp ^
src\tests\session_smoke\session_smoke_thread_cases.cpp ^
src\tests\session_smoke\vdtrace_session_smoke_test.cpp

set SESSION_SMOKE_OBJS=^
obj\release\vdtrace_session_smoke\session_smoke_runtime.obj ^
obj\release\vdtrace_session_smoke\session_smoke_runtime_entries.obj ^
obj\release\vdtrace_session_smoke\session_smoke_trace.obj ^
obj\release\vdtrace_session_smoke\session_smoke_trace_helpers.obj ^
obj\release\vdtrace_session_smoke\session_smoke_trace_workers.obj ^
obj\release\vdtrace_session_smoke\session_smoke_cli.obj ^
obj\release\vdtrace_session_smoke\session_smoke_config.obj ^
obj\release\vdtrace_session_smoke\session_smoke_trace_cases_basic.obj ^
obj\release\vdtrace_session_smoke\session_smoke_trace_cases_features.obj ^
obj\release\vdtrace_session_smoke\session_smoke_trace_cases_recorder.obj ^
obj\release\vdtrace_session_smoke\session_smoke_thread_cases.obj ^
obj\release\vdtrace_session_smoke\vdtrace_session_smoke_test.obj

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace\pch.obj /Fpobj\release\vdtrace\vdtrace.pch /Yc"pch.h" /c src\pch.cpp
if errorlevel 1 exit /b 1

cl /nologo /TC /MT /utf-8 /Iinclude /Iinclude\third_party\zydis /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace\Zydis.obj /c src\third_party\zydis\Zydis.c
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace\\ /Fpobj\release\vdtrace\vdtrace.pch /Yu"pch.h" /c %CORE_CPP%
if errorlevel 1 exit /b 1

lib /nologo /OUT:bin\release\VDTraceStatic.lib %CORE_OBJS%
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace\\ /Fpobj\release\vdtrace\vdtrace.pch /Yu"pch.h" /c src\VDTraceDllMain.cpp
if errorlevel 1 exit /b 1

link /nologo /DLL /OUT:bin\release\VDTrace.dll /IMPLIB:bin\release\VDTrace.lib /DEF:src\VDTrace.def %CORE_OBJS% obj\release\vdtrace\VDTraceDllMain.obj Psapi.lib
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace_agent\\ /Fpobj\release\vdtrace\vdtrace.pch /Yu"pch.h" /c %AGENT_CPP%
if errorlevel 1 exit /b 1

link /nologo /DLL /OUT:bin\release\VDTraceAgent.dll %CORE_OBJS% %AGENT_OBJS% Psapi.lib Advapi32.lib
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace_example\\ /c src\tools\vdtrace_example.cpp
if errorlevel 1 exit /b 1

link /nologo /OUT:bin\release\vdtrace_example.exe obj\release\vdtrace_example\vdtrace_example.obj bin\release\VDTraceStatic.lib Psapi.lib
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /Foobj\release\vdtrace_smoke\\ /c src\tools\VDTraceControlSupport.cpp src\tools\VDTraceControlSupportUtil.cpp src\tests\agent_smoke\vdtrace_agent_smoke_test.cpp
if errorlevel 1 exit /b 1

link /nologo /OUT:bin\release\vdtrace_agent_smoke_test.exe obj\release\vdtrace\pch.obj obj\release\vdtrace_smoke\VDTraceControlSupport.obj obj\release\vdtrace_smoke\VDTraceControlSupportUtil.obj obj\release\vdtrace_smoke\vdtrace_agent_smoke_test.obj obj\release\vdtrace\VDTraceIpc.obj Psapi.lib
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /Foobj\release\vdtrace_async_smoke\\ /c src\tests\async_smoke\vdtrace_async_handoff_smoke_test.cpp
if errorlevel 1 exit /b 1

link /nologo /OUT:bin\release\vdtrace_async_handoff_smoke_test.exe obj\release\vdtrace_async_smoke\vdtrace_async_handoff_smoke_test.obj bin\release\VDTraceStatic.lib Psapi.lib
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /Foobj\release\vdtrace_session_smoke\\ /c %SESSION_SMOKE_CPP%
if errorlevel 1 exit /b 1

link /nologo /OUT:bin\release\vdtrace_session_smoke_test.exe %SESSION_SMOKE_OBJS% bin\release\VDTraceStatic.lib Psapi.lib
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /Foobj\release\vdtrace_smoke_suite\\ /c src\tests\smoke_suite\vdtrace_smoke_suite_test.cpp
if errorlevel 1 exit /b 1

link /nologo /OUT:bin\release\vdtrace_smoke_suite_test.exe obj\release\vdtrace_smoke_suite\vdtrace_smoke_suite_test.obj
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace_decrypt_smoke\\ /Fpobj\release\vdtrace\vdtrace.pch /Yu"pch.h" /c src\tests\decrypt_smoke\VDTraceDecryptSmokeHelper.cpp src\tests\decrypt_smoke\VDTraceDecryptSmokeHelperRuntime.cpp
if errorlevel 1 exit /b 1

link /nologo /DLL /OUT:bin\release\VDTraceDecryptSmokeHelper.dll obj\release\vdtrace\pch.obj obj\release\vdtrace_decrypt_smoke\VDTraceDecryptSmokeHelper.obj obj\release\vdtrace_decrypt_smoke\VDTraceDecryptSmokeHelperRuntime.obj
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /Foobj\release\vdtrace_decrypt_smoke\\ /c src\tests\decrypt_smoke\vdtrace_decrypt_smoke_support.cpp src\tests\decrypt_smoke\vdtrace_decrypt_smoke_test.cpp
if errorlevel 1 exit /b 1

link /nologo /OUT:bin\release\vdtrace_decrypt_smoke_test.exe obj\release\vdtrace_decrypt_smoke\vdtrace_decrypt_smoke_support.obj obj\release\vdtrace_decrypt_smoke\vdtrace_decrypt_smoke_test.obj bin\release\VDTraceStatic.lib Psapi.lib
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace_trigger_wait\\ /Fpobj\release\vdtrace\vdtrace.pch /Yu"pch.h" /c src\tests\trigger_wait\VDTraceTriggerWaitHelper.cpp
if errorlevel 1 exit /b 1

link /nologo /DLL /OUT:bin\release\VDTraceTriggerWaitHelper.dll obj\release\vdtrace\pch.obj obj\release\vdtrace_trigger_wait\VDTraceTriggerWaitHelper.obj
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace_trigger_wait\\ /c src\tests\trigger_wait\vdtrace_trigger_wait_test.cpp src\tests\trigger_wait\vdtrace_rootstop_test.cpp src\tests\trigger_wait\vdtrace_stop_recovery_test.cpp
if errorlevel 1 exit /b 1

link /nologo /OUT:bin\release\vdtrace_trigger_wait_test.exe obj\release\vdtrace_trigger_wait\vdtrace_trigger_wait_test.obj bin\release\VDTraceStatic.lib Psapi.lib
if errorlevel 1 exit /b 1

link /nologo /OUT:bin\release\vdtrace_rootstop_test.exe obj\release\vdtrace_trigger_wait\vdtrace_rootstop_test.obj bin\release\VDTraceStatic.lib Psapi.lib
if errorlevel 1 exit /b 1

link /nologo /OUT:bin\release\vdtrace_stop_recovery_test.exe obj\release\vdtrace_trigger_wait\vdtrace_stop_recovery_test.obj bin\release\VDTraceStatic.lib Psapi.lib
if errorlevel 1 exit /b 1

if "%BUILD_FULL%"=="0" exit /b 0

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace_ctl\\ /c src\tools\VDTraceControlSupport.cpp src\tools\VDTraceControlSupportUtil.cpp src\tools\VDTraceControlSupportInject.cpp src\tools\vdtrace_ctl.cpp src\tools\vdtrace_ctl_parse.cpp src\tools\vdtrace_ctl_command.cpp
if errorlevel 1 exit /b 1

link /nologo /OUT:bin\release\vdtrace_ctl.exe obj\release\vdtrace\pch.obj obj\release\vdtrace_ctl\VDTraceControlSupport.obj obj\release\vdtrace_ctl\VDTraceControlSupportUtil.obj obj\release\vdtrace_ctl\VDTraceControlSupportInject.obj obj\release\vdtrace_ctl\vdtrace_ctl.obj obj\release\vdtrace_ctl\vdtrace_ctl_parse.obj obj\release\vdtrace_ctl\vdtrace_ctl_command.obj obj\release\vdtrace\VDTraceIpc.obj
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace_autostart\\ /Fpobj\release\vdtrace\vdtrace.pch /Yu"pch.h" /c src\autostart\VDTraceAutoStartConfig.cpp src\autostart\VDTraceAutoStartConfigText.cpp src\autostart\VDTraceAutoStartWait.cpp src\autostart\VDTraceAutoStartHelper.cpp src\autostart\VDTraceAutoStartHelperSupport.cpp
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /Foobj\release\vdtrace_autostart\\ /c src\tools\VDTraceControlSupport.cpp src\tools\VDTraceControlSupportUtil.cpp src\tools\VDTraceControlSupportInject.cpp src\tools\VDTraceLoaderControlSupport.cpp src\tools\vdtrace_autostart.cpp src\tools\vdtrace_autostart_io.cpp src\tools\vdtrace_autostart_install.cpp
if errorlevel 1 exit /b 1

link /nologo /DLL /OUT:bin\release\VDTraceAutoStart.dll obj\release\vdtrace\pch.obj obj\release\vdtrace_autostart\VDTraceAutoStartConfig.obj obj\release\vdtrace_autostart\VDTraceAutoStartConfigText.obj obj\release\vdtrace_autostart\VDTraceAutoStartWait.obj obj\release\vdtrace_autostart\VDTraceAutoStartHelper.obj obj\release\vdtrace_autostart\VDTraceAutoStartHelperSupport.obj obj\release\vdtrace_autostart\VDTraceControlSupport.obj obj\release\vdtrace_autostart\VDTraceControlSupportUtil.obj obj\release\vdtrace\VDTraceIpc.obj Psapi.lib Advapi32.lib
if errorlevel 1 exit /b 1

link /nologo /OUT:bin\release\vdtrace_autostart.exe obj\release\vdtrace\pch.obj obj\release\vdtrace_autostart\VDTraceAutoStartConfig.obj obj\release\vdtrace_autostart\VDTraceAutoStartConfigText.obj obj\release\vdtrace_autostart\VDTraceControlSupport.obj obj\release\vdtrace_autostart\VDTraceControlSupportUtil.obj obj\release\vdtrace_autostart\VDTraceControlSupportInject.obj obj\release\vdtrace_autostart\VDTraceLoaderControlSupport.obj obj\release\vdtrace_autostart\vdtrace_autostart.obj obj\release\vdtrace_autostart\vdtrace_autostart_io.obj obj\release\vdtrace_autostart\vdtrace_autostart_install.obj obj\release\vdtrace\VDTraceIpc.obj Psapi.lib Advapi32.lib
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace_early_loader\\ /Fpobj\release\vdtrace\vdtrace.pch /Yu"pch.h" /c src\early_loader\VDTraceEndfieldBaseProxy.cpp
if errorlevel 1 exit /b 1

link /nologo /DLL /OUT:bin\release\VDTraceEndfieldBaseProxy.dll obj\release\vdtrace\pch.obj obj\release\vdtrace_early_loader\VDTraceEndfieldBaseProxy.obj
if errorlevel 1 exit /b 1

cl /nologo /std:c++17 /MT /EHsc /utf-8 /Iinclude /Isrc /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS /DZYDIS_STATIC_BUILD /DZYCORE_STATIC_BUILD /Foobj\release\vdtrace_loader\\ /Fpobj\release\vdtrace\vdtrace.pch /Yu"pch.h" /c src\loader\WinHttpLoaderDllMain.cpp
if errorlevel 1 exit /b 1

link /nologo /DLL /IGNORE:4222 /OUT:bin\release\winhttp.dll obj\release\vdtrace\pch.obj obj\release\vdtrace_loader\WinHttpLoaderDllMain.obj
if errorlevel 1 exit /b 1

copy /Y "%SYSTEM_WINHTTP%" "bin\release\winhttp_original.dll" >nul
if errorlevel 1 exit /b 1
exit /b 0
