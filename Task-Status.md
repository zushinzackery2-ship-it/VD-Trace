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
  trace stops (step count reached), `Stop`s, flushes
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

### LiteTrace minimalism audit (thread/async knobs removed)

Reviewed the full `src/lite/` flow to confirm LiteTrace is a pure trigger-point
tracer with no leftover AutoStart/Agent logic:

- The path is already Agent/IPC/launch/wait free: `RunLiteTrace` reads the INI,
  builds `vdtrace::Options`, and drives `Session::Configure/Start` directly. No
  Agent DLL, no named-pipe IPC, no `[launch]`/`[wait]` sections.
- Removed the redundant thread-selection / cross-thread knobs from the LiteTrace
  config struct, parser, default template, and example INI:
  `thread_id`, `auto_select_thread`, `block_main_thread`, `async_thread_handoff`.
  Checked against the core `Start`/`Configure`: with a trigger set,
  `auto_select_thread` + `thread_id=0` is exactly the "trace whichever thread hit
  the breakpoint" mode (the core even forces `thread_id=0` in that case), so the
  knobs were pure noise for a pure-breakpoint tracer.
- `BuildTraceOptions` now hardcodes the lightweight semantics:
  `thread_id=0`, `auto_select_thread=true`, `block_main_thread=false`,
  `queue_trigger_threads=false`, `async_thread_handoff=false`. LiteTrace traces the
  single thread that reaches `trigger_point` and does not chase spawned threads,
  rotate threads, or block the main thread (all AutoStart/Agent-only behaviours).
- Old INIs that still contain the deleted keys keep working: unknown keys are
  ignored by the parser, so nothing breaks; they simply have no effect.
- Also removed the unsolicited `[lite]` `finish_timeout_ms` / `poll_interval_ms`
  keys (a self-added "safety valve" that was never requested). The bootstrap thread
  now simply polls `Session::IsRunning()` on a fixed 50 ms interval and waits for the
  trace to actually finish - there is no timeout. The only remaining `[lite]` key is
  `exit_process_on_finish`. Stop conditions are therefore just: `max_events` reached,
  or (optionally) `root_stop_on_return`, or the session stopping itself; then the
  process ends only if `exit_process_on_finish=true`.
- Kept legitimate trace-scope knobs (`modules`, `call_depth`/depth filters,
  `trace_outside_modules`, `repeat_hits`, `idle_escape_threshold`,
  `enhanced_sampling`, `probe_spec`, `root_stop_on_return`, `sim_fast_forward*`) -
  none of those are thread/agent cruft.
- Linux checks: MinGW `-std=c++20 -municode -Wall -Wextra` syntax-check of all five
  `src/lite` sources passed clean; line-count/brace-balance re-verified (max 192).

### LiteTrace step / specified modes

LiteTrace now runs in one of two explicit modes, selected by `[lite] mode`:

- `mode = step` (default) — trace from `trigger_point` for `max_events` steps, then
  stop. Every end-point knob is ignored: `end_point` is cleared and
  `root_stop_on_return` is forced off during config load (`ApplyLiteMode`), so a step
  run is bounded purely by the step count.
- `mode = specified` — trace from `trigger_point` until execution reaches `end_point`.
  `max_events` is forced to 0 (unlimited steps), so the step count is ignored. The run
  is bounded by the end point (and, if the user also set `root_stop_on_return`, by the
  root frame returning, whichever happens first). `BuildTraceOptions` rejects a
  `specified` config that has neither `end_point` nor `root_stop_on_return`, so an
  unbounded trace cannot be started by accident.

`end_point` reuses the `trigger_point` grammar (`Module.dll+0xRVA` /
`Module.dll!0xRVA` / `0xABSOLUTE`) and is parsed with the same `ParseLiteTriggerPoint`
helper into `Options::stop_module_name` + `Options::stop_address`.

Core support (minimal `Options` extension, shared by all trace consumers, opt-in and
backward compatible — disabled when `stop_address == 0` and `stop_module_name` empty):

- `include/VDTrace/VDTrace.h` — added `Options::stop_module_name` +
  `Options::stop_address` (RVA when a module name is set, otherwise absolute).
- `src/core/runtime/VDTraceInternal.h` / `VDTraceRuntimeConfig.cpp` — added
  `Session::Impl::resolved_stop_address` and `ResolveStopAddress` (mirrors
  `ResolveTriggerAddress`); `VDTraceRuntimeConfigure.cpp` resolves it in `Configure`.
- `src/core/hardware/VDTraceHardwareException.cpp` — DR backend: after the arriving
  edge is recorded, if the faulted RIP equals `resolved_stop_address` the session
  disarms and stops (checked just before the existing `max_events` stop, same pattern).
- `src/core/decoder/VDTraceDecoder.cpp` — TF backend: if the next instruction to
  execute is `resolved_stop_address`, stop before running it (checked ahead of the
  root-return / `max_events` branches).
- `src/core/runtime/VDTraceSession.cpp` — `DescribeState` now reports `stop=0x… / off`.

End-point matching granularity: in DR mode the stop fires when a traced control-flow
edge *arrives at* `end_point` (so pick a real edge target — a function entry, call
target, or branch target; the natural choice for an "end point"). In TF mode
(`backend = TF`) any instruction address matches exactly. Documented in the INI.

Linux checks: MinGW `-std=c++17 -fsyntax-only` on all edited core + lite sources
passed clean (the lone `std::memcpy` note in `VDTraceDecoder.cpp` is pre-existing and
comes from the standalone check not pulling `<cstring>` transitively the way the MSVC
PCH does — unrelated to this change; confirmed clean with `-include cstring`).

Windows-side verification still required (LiteTrace modes / stop address):

- Build the `LiteTrace` target and `VDTraceStatic` with MSVC and confirm the new
  `Options` fields and stop checks compile under `/W4 /permissive-`.
- `mode = specified`: inject with a valid `trigger_point` + `end_point`; confirm the
  trace stops when the end point is reached and the last recorded event is the
  transition into it (DR) / the instruction just before it (TF).
- `mode = step`: confirm `end_point` / `root_stop_on_return` are ignored and the trace
  stops exactly at `max_events`.
- Confirm a `specified` config with no end point is rejected with a clear log message
  rather than tracing forever.

## Simulated fast-forward (sim-skip) for the DR backend

Goal: stop paying a single-step/DR exception on control-flow edges whose outcome is
already certain, and only fault for real at genuinely uncertain points. This reuses
the extender `SimContext`/decoder instead of rewriting the VEH core.

New module `src/core/sim_fastforward/` (all files Allman, ≤300 lines):

- `VDTraceSimFastForward.h` — public entry `FastForwardDeterministicFlow`.
- `VDTraceSimFastForwardInternal.h` — `PredictedJump`, `WalkState`, budget constant,
  helper declarations.
- `VDTraceSimFastForward.cpp` — the bounded synchronous walk: seeds an optional
  `SimContext`, follows deterministic jump edges, buffers predicted edges, and stops
  at the frontier / cycle / budget / `max_events`.
- `VDTraceSimFastForwardResolve.cpp` — `TryPlanSkippableJump`: block analysis, the
  region/probe guards, interior effect replay, and register-indirect resolution with
  the purity guard.
- `VDTraceSimFastForwardEmit.cpp` — `EmitPredictedJump`: builds the `Jump` `StepEvent`
  and routes it through the shared `hardware_transition_detail::ShouldEmitTransition`
  dedup + module labeling + callback, exactly like a real DR transition.

Integration point: a single pre-pass at the top of `ProgramHardwareObservationImpl`
(`src/core/hardware/VDTraceHardwareSupport.cpp`), the one choke point that arms the
next DR observation. When `options.sim_fast_forward` is set it advances `entry` to the
frontier and emits the predicted edges; the existing arming logic then arms real
hardware on the frontier block. Nothing else in the VEH/DR core changed.

Why it is correct-by-construction (verified against the code, not just asserted):

- Only unconditional `Jump` edges are skipped. Their target is either encoded in the
  instruction (`jmp rel/imm`) or, with `sim_fast_forward_indirect`, computed purely in
  registers (`jmp reg`, guarded by `SimContext.known` + a "no memory read / no
  unmodeled instruction" window flag). In both cases the CPU is guaranteed to follow
  the predicted target, so arming the later frontier cannot diverge.
- The CPU still executes every skipped instruction natively — no register/memory
  writeback is performed. Only the redundant per-jump exceptions are removed.
- `ApplyHardwareContextObservations` rebuilds `Dr7`/`Dr6` from scratch each arm, so no
  stale breakpoint exists at a skipped intermediate target.
- `UpdateCallStackForTransition` is a no-op for `Jump`, so predicted jumps never
  disturb the shadow call stack / depth.
- Conditional branches, calls, returns, indirect-through-memory, syscalls, interrupts,
  truncated blocks, value-probe ranges, TF-filter/system/untracked targets, and any
  unknown register all remain frontiers → they fault for real exactly as today.

Toggles (both default off, so the mode is a strict no-op unless enabled):

- `Options::sim_fast_forward` — direct-jump chain skipping (zero misprediction risk).
- `Options::sim_fast_forward_indirect` — additionally resolve register-indirect jumps
  via `SimContext` (provably equal to the CPU result under the purity guard; flagged
  for Windows validation because a misprediction there would mean silent trace loss,
  not a crash).

Config wiring: exposed through the in-process LiteTrace INI (`[trace]` keys
`sim_fast_forward`, `sim_fast_forward_indirect`) — the path that maps an INI directly
onto `Options`. The Agent IPC `Configure` wire format was intentionally left unchanged
(adding these to autostart/GUI would require a protocol field; noted below).

Relationship to `hot_bypass`: complementary, not overlapping. `hot_bypass` drops a
*hot repeated* data-dependent edge (loop back-edge) after it has faulted
`hot_bypass_threshold` times and re-arms at the loop exit — reactive, count-based,
`FirstSeen`-only. Fast-forward drops a *statically certain* edge on the first
encounter, no repetition needed. Both continue to run: fast-forward removes the
"known" edges up front, `hot_bypass` still handles the "unknown-but-repetitive" ones.

Linux-side verification performed:

- Line-count + brace-balance on all five new files (max 149 lines).
- MinGW-w64 (`x86_64-w64-mingw32-g++ -std=c++20 -municode -Wall -Wextra`) compiled all
  three new `.cpp` to object files and the modified `VDTraceHardwareSupport.cpp`,
  clean. `LiteTraceConfig*.cpp` / `LiteTraceRuntime.cpp` re-checked with the new keys.
- `nm -uC` on the new objects: the only undefined `vdtrace::*` symbols are the
  extender `detail` (`InitializeContext`/`ApplyInstructionEffects`/`ReadRegister`/
  `DecodeFullInstruction`), the hardware helpers (`AnalyzeBasicBlock`,
  `FindModuleRange`, `HasValueProbeInRange`, `ResolveExecutionModeForAddress`,
  `CaptureThreadContext`) and `hardware_transition_detail::ShouldEmitTransition` /
  `ResetSuppressedTransitionState` — all provided by `VDTraceStatic`. No new deps.

Windows-side verification still required (sim fast-forward):

- MSVC `/W4 /permissive-` build + link of `VDTraceStatic`/`VDTrace` with the new module.
- Functional: with `sim_fast_forward=true` on a DR (`control_flow_only`) trace, confirm
  the recorded jump edges match a baseline (`sim_fast_forward=false`) run exactly
  (same source/target/order under `FirstSeen`), while the DR exception count drops on
  jump-heavy/flattened code. Predicted jump events legitimately carry an invalid
  `thread_context` (no live snapshot); confirm downstream consumers tolerate that
  (the recorder's extended per-block memory analysis already gates on
  `entry_context.valid`).
- With `sim_fast_forward_indirect=true`, validate on code containing register-indirect
  jumps that the resolved targets match actual execution and no trace divergence/stall
  occurs; keep it off until validated.
- Follow-up (not done here): thread `sim_fast_forward*` through the Agent IPC
  `Configure` payload + autostart `[trace]` + GUI so non-LiteTrace launch paths can
  toggle it. This needs an IPC protocol field, so it was deferred to avoid changing
  the stable wire format.

## Project tidy + LiteTrace release packaging

Tidy pass over the `experimental` branch (structure + doc alignment, no functional
code deleted — the BepInEx plugin and every CMake target are intentional and stay):

- Documented what was previously undocumented so the top-level docs match the tree:
  added `LiteTrace` (`src/lite/`, `LiteTrace.dll`) and the sim fast-forward module
  (`src/core/sim_fastforward/`) to both `README.md` and `README_CN.md` — feature
  summary, entry points, source layout, build-products table, a build/release
  subsection, and the limitations table (DR vs TF end-point precision).
- Aligned the repository-policy text with the actual `.gitignore`: it now lists
  `release/` as tracked and `bin/`/`obj/`/`dist/` as ignored, and drops the stale
  `docs/ tools/ ref_pic/ backup/` names that no longer exist. Noted the two tracked
  `LiteTrace.ini` templates as explicit exceptions to the global `*.ini` ignore.

LiteTrace release package (source-of-truth committed; the binary is built on
Windows and never committed):

- `release/LiteTrace/README.md` — full Chinese usage guide + English brief: what
  LiteTrace is, differences vs VDTrace/AutoStart, the two modes (step / specified)
  with INI examples, injection + directory placement, `trigger_point`/`end_point`
  format, a complete `[trace]`/`[lite]` config table, notes (DR vs TF end-point
  precision, fixed thread semantics, no timeout, relative paths), Windows/VS2022
  build steps, and known limitations.
- `release/LiteTrace/LiteTrace.ini` — ready-to-edit config template (mirrors the
  canonical `src/tools/examples/LiteTrace.ini`; release default
  `exit_process_on_finish = false`).
- `release/LiteTrace/PLACEHOLDER-LiteTrace.dll.txt` — marks where the built DLL goes
  and how to produce it (binaries are not committed on this branch).
- `release/LiteTrace/build-and-package.bat` — one-click Windows build: locates VS2022
  via `vswhere` (fallback to an editable `VSPATH`), CMake configure, builds only the
  `LiteTrace` target (pulls in `VDTraceStatic`), then calls the packager.
- `release/LiteTrace/package.ps1` — stages `LiteTrace.dll` + `LiteTrace.ini` +
  `README.md` (and `VDTrace.dll` if present) into `dist\LiteTrace-v<version>\` and
  zips it; errors clearly if the DLL has not been built yet.
- `.gitignore` — whitelist-style repo, so added `!/release/`, `dist/`, and
  `!/release/LiteTrace/LiteTrace.ini`. Verified with `git check-ignore` that all five
  release files are tracked and that `dist/` stays ignored.

Linux-side verification performed:

- `git check-ignore` on every intended release file — none ignored; `dist/` confirmed
  ignored. Scripts are text-only; no compilation involved. READMEs are docs only.

Windows-side verification still required (release packaging):

- Run `release\LiteTrace\build-and-package.bat` on a VS2022 x64 machine and confirm it
  produces `bin\release\LiteTrace.dll` and stages `dist\LiteTrace-v0.1.0\` + the zip.
- Confirm `vswhere` discovery works (or the `VSPATH` fallback needs editing for the
  local install).

## hot_bypass default: off (enable with 8)

Per user request: `hot_bypass` / `idle_escape_threshold` is now **disabled by default**
(`0`). To enable, set `idle_escape_threshold = 8` in INI (recommended value when on).

Unified sites:
- Core `Options::hot_bypass_threshold = 0`; IPC + vdtrace_ctl default 0.
- LiteTrace / autostart config structs + missing-key parse fallback `0`.
- GUI: `idleEscapeEnabled` default false; threshold field still defaults to `8` when
  the user toggles the switch on.
- Templates: `idle_escape_threshold = 0` with comment "set 8 to enable".
- Session-smoke hot-loop cases explicitly set `hot_bypass_threshold = 8` (feature is
  opt-in now, tests still validate bypass behaviour).

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
