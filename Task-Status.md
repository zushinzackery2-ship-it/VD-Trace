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
