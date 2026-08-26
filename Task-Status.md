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

## Conventions honored

- Allman brace style throughout Dart sources.
- No source file exceeds 300 lines; larger units split into dedicated files/folders.
- No cross-project file references.
- Committer identity: Vernal <zushinzackery2@gmail.com>.

## Not done (needs a Windows toolchain)

- Building/validating the C++ core, Agent, CLI tools and smoke suite (MSVC + Windows SDK).
- Building the Flutter Windows runner and the BepInEx plugin (.NET).
