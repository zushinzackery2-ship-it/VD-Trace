# VD-Trace Task Status

Branch: `experimental`
Owner: Vernal <zushinzackery2@gmail.com>

## Goal

Refactor the project, fix bugs, optimize lifecycle, and ship a better looking GUI.

## Scope decision

The C++ core (`src/core`, `src/agent`, `src/control`, `src/autostart`, `src/tools`,
`src/tests`) is a Windows x64 kernel-adjacent tracer (VEH, DR registers, named-pipe IPC,
PE dump/fix). It can only be built and validated on Windows with Visual Studio 2022 and
the Windows SDK. This refactor was produced in a Linux container without MSVC, so a blind
rewrite of that untestable code would risk breaking a working toolkit. This iteration
therefore delivers a complete, verified overhaul of the layer that was explicitly
requested and that can be validated here — the Flutter GUI — plus concrete bug fixes and
lifecycle hardening in that layer. A full Flutter 3.47.1 / Dart 3.13.1 toolchain was
installed in the container so `flutter analyze` and `flutter test` actually run (the
`win32` package is pure Dart FFI, so the non-Windows tests execute on Linux). Every
touched file is left in a finished, coherent state — no half versions.

## Work log — GUI (`src/flutter_gui`)

Done and verified:

- New tokenized Material 3 dark theme + palette (`lib/src/theme/`).
- Reusable widget kit: section cards, form fields, action buttons, status chips,
  console panels, icon actions (`lib/src/widgets/`).
- New shell: frameless custom title bar, left navigation rail, live target/status
  header, module picker (`lib/src/shell/`). The `WM_APP` window-message and drag
  contract with `windows/runner/win32_window.cpp` is preserved.
- The eight configuration sections are now dedicated page widgets (`lib/src/pages/`).
  Only the active section is rendered (with a fade), matching the previous `TabBarView`
  behavior and keeping every file under 300 lines.
- Controller lifecycle refactor (`lib/src/vdtrace_controller*.dart`):
  - Self-scheduling, non-overlapping poll loop (was `Timer.periodic`).
  - Status/module probes are skipped while a user action holds the Agent so two
    clients never hit the same Agent pipe at once.
  - Module list re-enumerated only on Agent offline→online transitions (or when
    empty) instead of on every tick.
  - Post-dispose `notifyListeners` guard.
  - Public API kept stable so existing controller/loader tests still pass; workflow
    actions moved into a part file to keep the class under 300 lines.
- Service split: Loader message decoding → `loader_message_codec.dart`; pipe server
  create/close → `loader_pipe_transport.dart`; `loader_bridge.dart` trimmed < 300 lines.

Bug fixes:

- `defaultOutputPath` placed the pid inside a raw string, so `$pid` was emitted
  literally and auto-named trace paths were never recognized as "auto". Now the pid
  is interpolated and matches `isAutoOutputPath`.
- Removed dead code (`TextPanel.subtitle: actions.isEmpty ? null : null`) as part of
  the widget-kit rewrite.

Tests:

- Kept the existing widget and controller-workflow regression tests green.
- Added `output_path_test.dart` (output-path fix) and `navigation_test.dart`
  (nav-rail section switching).

Verification (Flutter 3.47.1):

- `flutter analyze` → No issues found.
- `flutter test` → 6 passed, 3 skipped (the Windows-only real-Agent Loader tests).

## Work log — C++ business layer (`src/core`, `src/agent`, `src/control`, `src/tests`)

Second iteration, on the same `experimental` branch. The C++ layer was already
cleanly modularized (each subsystem uses a `X.cpp` + `X.h` + `XInternal.h` +
`XSupport.cpp` split, Allman braces throughout), so the responsible work here was
to enforce the project's own rules across the whole layer, fix the concrete defect
the review turned up, and verify everything that can be verified without MSVC —
rather than blind-rewriting a working, untestable Windows tracer.

Bug / lifecycle fix:

- `WaitForLoaderSessionInternal` (loader-session IPC) computed the
  `ConnectNamedPipe` wait timeout as `deadline - GetTickCount64()` on unsigned
  `ULONGLONG`. If the tick counter reached the deadline between the loop guard and
  that computation, the subtraction wrapped to a huge value, was capped to
  `0xFFFFFFFF`, and cast to `DWORD` — which `WaitForSingleObject` treats as
  `INFINITE`, hanging the caller. Now computed with an explicit `now < deadline`
  guard so an elapsed deadline yields `0` and the loop exits cleanly.

300-line rule — six oversized files split into cohesive modules (each unit now
< 300 lines), matching the existing `Internal.h` + `Support`/`Format` convention:

- `src/agent/VDTraceAgentMemorySupport.cpp` → kept address parsing/resolution +
  byte writing; preview formatting moved to `VDTraceAgentMemoryFormat.cpp`.
- `src/agent/VDTraceAgentDump.cpp` → module enumeration / image copy / PE path
  helpers moved to `VDTraceAgentDumpSupport.cpp` behind the new
  `VDTraceAgentDumpInternal.h` (`vdtrace::agent::dump_detail`).
- `src/core/depth_filter/VDTraceDepthFilter.cpp` → spec tokenizer/parser helpers
  moved to `VDTraceDepthFilterParse.cpp` behind the new
  `VDTraceDepthFilterInternal.h` (`vdtrace::depth_filter_detail`).
- `src/core/extender/VDTraceExtenderProcess.cpp` → event/section formatting moved
  to `VDTraceExtenderProcessFormat.cpp` (declarations already in the existing
  `VDTraceExtenderInternal.h`).
- `src/tests/decrypt_smoke/VDTraceDecryptSmokeHelperRuntime.cpp` → the JIT stage
  emitter (`CodeWriter` + `BuildDynamicStage`) moved to
  `VDTraceDecryptSmokeHelperJit.cpp` (`decrypt_smoke_helper::jit_detail`).
  `BuildDynamicStage` now returns the executable buffer + function pointer via
  out-parameters so the runtime globals and every address-taking export stay in
  the runtime TU. Emitted machine-code bytes are unchanged.
- `src/tests/trigger_wait/vdtrace_trigger_wait_test.cpp` → path/log/event-line
  helpers moved to `vdtrace_trigger_wait_support.{h,cpp}` (`trigger_wait_test`).
  The traced workload functions and their file-local globals stay in the test TU
  so tracing semantics are unaffected.

`CMakeLists.txt` updated with every new translation unit. No public API, IPC
protocol (`VDTraceIpc.h`, loader `MessageKind`/payloads), enum, or on-wire layout
was changed — the agent IPC server, control client, thread enumeration
(owner-PID-filtered) and DllMain lifecycle were reviewed and left intact.

Verification done here (Linux container, no MSVC):

- Structural verifier over the whole non-third-party C++ layer: no file > 300
  lines; balanced braces/parentheses (comment/string-aware); every header carries
  an include guard.
- Split integrity: each moved helper has exactly one definition, present in its
  new module and removed from the original; every new `.cpp` is wired into
  `CMakeLists.txt`; declarations in the new/`*Internal.h` headers match the
  definitions.
- Logic review of the touched IPC/lifecycle/threading paths.

## Work log — LiteTrace (`src/lite`)

New lightweight, self-contained tracer DLL: inject `LiteTrace.dll`, it reads
`LiteTrace.ini` by relative path, waits at the configured trigger, records the
trace, and can end the process afterwards. It is deliberately lighter than
`VDTraceAutoStart` — no `[launch]`/`[wait]` sections, no game launching, no Agent
DLL, and no named-pipe IPC.

Architecture:

- Core is reused, not rewritten: `LiteTrace` links `VDTraceStatic` and drives the
  existing in-process `vdtrace::Session` / `vdtrace::Options` API with a
  `vdtrace::TextFileRecorder` sink. VEH/DR internals are untouched.
- INI parsing is reused from autostart: `VDTraceAutoStartConfigText.cpp` (the
  generic `vdtrace::autostart::detail` trim/read/write/section/bool/uint64/
  call-depth helpers) is compiled into the `LiteTrace` target and called directly,
  so there is no second INI parser.

Modules (all Allman, all ≤ 300 lines, in `src/lite/`):

- `LiteTraceConfig.h` — `LiteTraceConfig` struct (the `[trace]` knobs ported from
  the autostart `[trace]` section / `Options`, plus the `[lite]` runtime knobs) and
  the public declarations.
- `LiteTraceConfig.cpp` — `LoadLiteTraceConfig`: writes the default template when
  the file is missing, parses `[trace]` + `[lite]`, validates backend/call-depth/
  execution-mode.
- `LiteTraceConfigParse.cpp` — `SplitModuleNames`, `ParseLiteTriggerPoint`,
  `BuildLiteDepthFilterSpec`, `ResolveLiteOutputPath`.
- `LiteTraceConfigText.cpp` — DLL-directory lookup, `DefaultLiteTraceConfigPath`
  (cwd → DLL directory), and the default INI template text.
- `LiteTraceRuntime.{h,cpp}` — builds `Options` from the config, opens the
  recorder, `Session::Configure`/`Start`, polls `Session::IsRunning()` until the
  trace stops (step count reached, or `finish_timeout_ms` elapsed), `Stop`s, flushes
  the recorder (destruct drains + `FlushFileBuffers`), then `ExitProcess` when
  `exit_process_on_finish` is set. Writes a `traces/LiteTrace-<pid>.log` diagnostic.
- `LiteTraceMain.cpp` — `DllMain(PROCESS_ATTACH)` calls `DisableThreadLibraryCalls`
  and starts one background bootstrap thread (avoids loader lock). Also exports
  `vdtrace_lite_bootstrap` for manual triggering. A one-shot atomic guards against
  double start.

Mandatory config items: `max_events` under `[trace]` is the step count;
`exit_process_on_finish` under `[lite]` ends the process when the trace finishes.

Products:

- `LiteTrace.dll` — `add_library(LiteTrace SHARED …)` in `CMakeLists.txt`, links
  `VDTraceStatic`, output to `bin/release`.
- Example config `src/tools/examples/LiteTrace.ini`.

Trigger contract: `trigger_point` (`Module.dll+0xRVA` / `Module.dll!0xRVA` /
`0xABSOLUTE`) is parsed into `Options.trigger_module_name` + `Options.trigger_address`
as a module + **RVA offset** (not a pre-resolved absolute). This matches
`ResolveTriggerAddress` in `src/core/runtime/VDTraceRuntimeConfig.cpp` (base is added
at configure time when a module name is present) and the working
`vdtrace_trigger_wait_test` reference.

`DllMain` note: `VDTraceStatic` also contains a `DllMain` (from
`src/core/api/VDTraceDllMain.cpp`). `LiteTrace` provides its own `DllMain` as a
direct target object, so the linker resolves `DllMain` from `LiteTraceMain.obj` and
never extracts the library's `DllMain` object — no duplicate symbol.

Verification done here (Linux container, no MSVC):

- MinGW-w64 cross toolchain installed; every `src/lite/*.cpp` and the reused
  `VDTraceAutoStartConfigText.cpp` pass `-fsyntax-only` **and** compile to object
  code (`-c`, template instantiation of `Session`/`Options`/`TextFileRecorder`
  succeeds) against the real Win32 headers, C++20, with the project's defines and
  `-include src/pch.h`.
- `-Wall -Wextra` clean on all lite sources.
- Undefined-symbol analysis (`nm -u`) confirms the lite objects reference only the
  autostart `detail::*` helpers (satisfied by the reused config-text TU in the
  target) and the public `vdtrace::Session` / `TextFileRecorder` API (satisfied by
  `VDTraceStatic`) — the CMake linkage is complete, nothing is missing.
- Line-count and brace-balance checks on every lite file (max 196 lines).

Windows-side verification still required (LiteTrace):

- `cmake --build` the `LiteTrace` target with MSVC `/W4 /permissive-` and confirm it
  links against `VDTraceStatic` with no duplicate/missing `DllMain` symbol.
- Inject `LiteTrace.dll` into a target: confirm the INI is found (cwd → DLL folder),
  the default template is written when missing, the trigger breakpoint arms and
  fires, `max_events` stops the trace, the log is written, and
  `exit_process_on_finish` ends the process cleanly after the recorder flushes.

## Conventions honored

- Allman brace style throughout Dart sources.
- No source file exceeds 300 lines; larger units split into dedicated files/folders.
- No cross-project file references.
- Committer identity: Vernal <zushinzackery2@gmail.com>.

## Windows-side verification still required (no MSVC in this container)

The C++ changes were reviewed and checked structurally, but must be built and run
on Windows with Visual Studio 2022 + the Windows SDK before shipping:

- `cmake --build` of every target (`VDTrace`, `VDTraceStatic`, `VDTraceAgent`,
  `VDTraceAutoStart`, CLI tools, all smoke tests). Confirms the six split
  translation units and their new internal headers compile and link with no
  duplicate/missing symbols under `/W4 /permissive-`.
- Run the smoke suite (`vdtrace_smoke_suite_test` and the individual
  session/async/agent/trigger-wait/rootstop/stop-recovery/decrypt tests) to
  confirm no behavior regressed from the refactor. The decrypt-smoke helper in
  particular relies on exact emitted machine code and function addresses — verify
  its encrypt/decrypt round-trip and the reported stage addresses still match.
- Exercise the loader-session IPC path (GUI/CLI attaching to a target) to confirm
  the `WaitForLoaderSessionInternal` timeout fix behaves correctly at and past the
  deadline.
- Building the Flutter Windows runner and the BepInEx plugin (.NET).
