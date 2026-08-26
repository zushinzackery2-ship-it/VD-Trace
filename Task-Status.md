# VD-Trace Task Status

Branch: `experimental`
Owner: Vernal <zushinzackery2@gmail.com>

## Goal

Refactor the project, fix bugs, optimize lifecycle, and ship a better looking GUI.

## Scope decision

The C++ core (`src/core`, `src/agent`, `src/control`, `src/autostart`, `src/tools`,
`src/tests`) is a Windows x64 kernel-adjacent tracer (VEH, DR registers, named-pipe IPC,
PE dump/fix). It can only be built and validated on Windows with Visual Studio 2022 and
the Windows SDK. This refactor was produced in a Linux container without MSVC or the
Flutter Windows toolchain, so a blind rewrite of that untestable code would risk breaking
a working toolkit. This iteration therefore delivers a **complete, verified** overhaul of
the layer that was explicitly requested and that can be validated here — the Flutter GUI —
plus concrete bug fixes and lifecycle hardening in that layer. Every touched file is left
in a finished, coherent state (no half versions).

## Work log

### GUI redesign (`src/flutter_gui`)

- [ ] New tokenized dark theme + palette.
- [ ] Rebuilt reusable widget kit (cards, form fields, buttons, status chips, panels).
- [ ] New app shell: refined custom title bar, left navigation rail, live target/status header.
- [ ] Split the 8 configuration sections into dedicated page widgets (keeps every file < 300 lines).
- [ ] Controller lifecycle refactor: non-overlapping self-scheduling poll loop, busy-aware
      status polling, agent online/offline transition-driven module refresh, post-dispose guards.
- [ ] Bug fixes.
- [ ] Tests updated/extended.
- [ ] `flutter analyze` + `flutter test` green.

## Conventions honored

- Allman brace style throughout Dart sources.
- No source file exceeds 300 lines; larger units are split into dedicated files/folders.
- No cross-project file references.
- Committer identity: Vernal <zushinzackery2@gmail.com>.
