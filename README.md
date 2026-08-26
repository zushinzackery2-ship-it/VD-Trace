<div align="center">

# VD-Trace

**Windows x64 Runtime Control-Flow Tracing and Diagnostics Toolkit**

*Hardware breakpoint tracing | DR-first backend with TF fallback | Agent IPC | Dump+Fix | Flutter GUI*

![C++](https://img.shields.io/badge/C%2B%2B-20-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey?style=flat-square)
![Toolchain](https://img.shields.io/badge/Toolchain-Visual%20Studio%202022-green?style=flat-square)
![Disasm](https://img.shields.io/badge/Disassembler-Zydis-orange?style=flat-square)

[中文文档](README_CN.md)

</div>

---

## Overview

VD-Trace is an open-source Windows x64 runtime tracing and diagnostics toolkit for authorized debugging, research, and reproducible native process analysis.

It combines a DR0-DR3 hardware-breakpoint tracing backend, localized Trap Flag fallback, Windows VEH dispatch, an in-process Agent, named-pipe IPC control, runtime module Dump+Fix, memory inspection, autostart workflows, CLI automation, and a Flutter Windows GUI.

The project is designed as reusable tracing infrastructure rather than a target-specific tool. Current public workflows are centered around VDTrace naming, configurable profiles, and generic Loader/Agent control paths.

---

## What VD-Trace Helps With

| Problem | VD-Trace capability |
|:--|:--|
| Runtime control flow is hard to reproduce | Captures basic-block-level control-flow edges and call/return transitions |
| Full single-step tracing is too expensive | Uses hardware breakpoints as the primary path and Trap Flag only where needed |
| Dynamic or anonymous executable pages need finer observation | Supports per-region depth filters and localized TF tracing |
| No source-level logs are available | Uses an in-process Agent to expose controlled runtime diagnostics |
| Runtime modules need post-processing before analysis | Dumps loaded modules and repairs PE layout for follow-up tooling |
| Manual setup is repetitive | Provides CLI, Flutter GUI, Loader Control IPC, and BepInEx/autostart workflows |
| Trace output can explode in hot loops | Includes hot-path bypass logic to reduce repetitive trace noise |

---

## Feature Summary

| Area | Capability |
|:--|:--|
| Control-flow tracing | DR0-DR3 execution breakpoints, basic-block edge capture, call/return tree output |
| Trap Flag fallback | Localized single-step mode for anonymous pages, probe stepping, write watch, and indirect edges |
| Depth filtering | Independent policies for traced modules, outside modules, anonymous executable pages, and selected DLLs |
| Triggered tracing | Starts trace collection only after a configured address or `module!RVA` is reached |
| Thread handling | Automatic thread capture, active-thread promotion, async handoff, and queued trigger handling |
| Probe observation | Register capture, fixed-address memory capture, pointer-relative memory capture, step mode, write-watch mode |
| Enhanced sampling | Captures before/after previews around selected cross-module call/return transitions |
| Static references | Extracts RIP-relative memory references and emits companion static reference metadata |
| Async recording | Preallocated ring buffer, formatting worker, writer worker, and backpressure control |
| Agent IPC | Configure, start, stop, status, list modules, dump module, read memory, write memory, shutdown |
| Dump+Fix | Exports loaded modules and repairs runtime PE layout for downstream analysis tools |
| HeapPeek | Tracks selected heap activity and memory changes during runtime observation |
| Extender | Pluggable event processing layer for future custom analysis outputs |
| GUI workflow | Flutter Windows GUI for target discovery, Agent loading, module refresh, Dump+Fix, trace control, and output viewing |
| CLI workflow | Deterministic command-line control through `vdtrace_ctl.exe` and `vdtrace_autostart.exe` |
| Autostart | BepInEx plugin and activation-file based autostart for IL2CPP/BepInEx application scenarios |
| LiteTrace | Single inject-and-go `LiteTrace.dll`: reads `LiteTrace.ini`, triggers at `trigger_point`, traces, no Agent/IPC; step / specified modes |
| Sim fast-forward | DR backend skips redundant single-step exceptions on deterministic direct jumps via `SimContext`; instructions still execute for real |

---

## Main Workflow

```text
Flutter GUI / CLI / autostart
    │
    ├─ Loader Control IPC: \\.\pipe\VDTraceLoaderControl
    │      └─ requests target-side loading of VDTraceAgent.dll
    │
    └─ Agent IPC: \\.\pipe\VDTrace-<pid>
           ├─ configure / start / stop / status
           ├─ modules / dump
           ├─ memory read / memory write
           └─ shutdown

Target process
    ├─ VDTraceAgent.dll
    └─ VDTrace core runtime
           ├─ DR hardware-breakpoint backend
           ├─ localized TF fallback backend
           ├─ VEH exception dispatch
           ├─ depth filters / probes / static refs / HeapPeek / Extender
           └─ ring buffer → formatter worker → writer worker
```

VD-Trace supports three practical usage modes:

1. **Interactive workflow** through the Flutter Windows GUI.
2. **Scripted workflow** through deterministic CLI commands.
3. **Autostart workflow** through BepInEx plugin activation and `vdtrace_autostart.exe`.

---

## Core Tracing Model

### DR-first tracing

The normal tracing path uses x64 hardware debug registers. VD-Trace places execution breakpoints on control-flow boundaries and records transitions when the traced thread reaches those points.

This keeps normal module tracing lighter than full single-step execution while preserving a useful runtime control-flow view.

### Local TF fallback

Trap Flag mode is used only for localized cases where hardware breakpoints are not enough or too coarse:

- anonymous executable pages
- probe step mode
- write-watch mode
- selected depth-filter regions
- short indirect-edge windows

After the local window ends, VD-Trace restores hardware-breakpoint tracing.

### Depth filters

Depth filters keep trace output focused on relevant code regions.

Example:

```text
depthfilter=outside=2:edge,anon=all:tf,module=TargetModule.dll:all:tf
```

| Rule | Meaning |
|:--|:--|
| `outside=<depth>[:edge\|tf]` | Policy for executable PE regions outside selected traced modules |
| `anon=<depth>[:edge\|tf]` | Policy for anonymous executable memory such as JIT/codegen pages |
| `module=<name>:<depth>[:edge\|tf]` | Independent policy for a selected DLL |

### Trigger point

A trigger point can delay trace collection until a specific absolute address or `module!RVA` is reached. This avoids startup noise and makes focused runtime experiments easier to reproduce.

### Hot-path bypass

Tight loops can produce excessive repeated events. VD-Trace detects repeated branch hits above a threshold and temporarily moves attention to the loop exit point, reducing trace explosion while preserving transition behavior.

---

## Runtime Diagnostics

After `VDTraceAgent.dll` is loaded into a target process, control clients can communicate with it through a named pipe.

| Command group | Purpose |
|:--|:--|
| Ping / Status | Check Agent availability and session state |
| Configure | Send trace settings, module filters, output path, trigger point, and probe settings |
| Start / Stop | Start or stop a trace session |
| ListModules | Enumerate loaded modules in the target process |
| DumpModule | Export and repair a loaded PE module |
| ReadMemory | Read target memory for diagnostics |
| WriteMemory | Write target memory in authorized debugging scenarios |
| Shutdown | Stop the Agent IPC service |

---

## Dump+Fix

VD-Trace includes a runtime module dumping path in the Agent. It exports a loaded module from the target process and repairs common runtime PE layout issues so the output is more suitable for tools such as IDA or Ghidra.

This is useful when the on-disk file is not enough to understand the loaded runtime image.

---

## Flutter GUI

The Flutter Windows GUI is the current primary graphical control surface. It uses
a frameless dark "diagnostics console" shell: a custom title bar, a live target/
status header, and a left navigation rail that switches between eight sections
(Core, Policy, Depth Filter, Observer, Memory, Trace, Log, Modules).

| GUI workflow | Description |
|:--|:--|
| Automatic target discovery | Finds Loader Control sessions and presents the current target |
| Live status header | Shows PID, trace/write state, backend and session count as status chips |
| Agent loading | Sends `LoadDllRequest` to load `VDTraceAgent.dll` into the target |
| Agent readiness checks | Ensures the Agent is online before dump, memory, or trace operations |
| Adaptive polling | Self-scheduling runtime poll that pauses probes during user actions and refreshes modules on Agent online/offline transitions |
| Module refresh | Loads the real module list from the target process |
| Dump+Fix | Selects a real module and exports a repaired dump |
| Trace preview | Live tail of the trace output file with copy and address→memory helpers |
| Output view | Displays CLI/Agent results and workflow status |
| Regression coverage | Loader IPC simulation, controller workflow, navigation and output-path tests |

---

## CLI Tools

| Tool | Role |
|:--|:--|
| `vdtrace_ctl.exe` | Agent IPC client for inject, configure, start, stop, modules, dump, read, write, and status workflows |
| `vdtrace_autostart.exe` | Autostart helper for plugin deployment, target launch, and trace completion waiting |
| `vdtrace_example.exe` | Example control client |

The CLI path is intended for reproducible experiments, automation, and regression testing.

---

## Autostart and BepInEx

VD-Trace supports a generic BepInEx/autostart workflow for IL2CPP/BepInEx application scenarios.

The autostart path is configuration-driven:

- activation file provides helper/config/log paths
- BepInEx plugin loads the VDTrace autostart helper
- autostart CLI can deploy the plugin and launch the target
- default public configuration does not bind to a single process name, game name, module, or fixed RVA

---

## Source Layout

| Path | Responsibility |
|:--|:--|
| `include/VDTrace/` | Public C/C++ API headers |
| `include/third_party/zydis/` | Zydis public header |
| `src/core/runtime/` | Session lifecycle, runtime state, configuration |
| `src/core/hardware/` | DR/TF backend, exception handling, hardware context transitions |
| `src/core/decoder/` | Instruction decoding and control-flow classification |
| `src/core/depth_filter/` | Depth-filter parsing and runtime policy resolution |
| `src/core/probe/`, `src/core/observer/` | Probe and observation rules |
| `src/core/sampling/`, `src/core/preview/` | Enhanced sampling and output previews |
| `src/core/static_refs/` | Static reference extraction and metadata output |
| `src/core/recorder/` | Ring buffer, formatter worker, writer worker |
| `src/core/extender/` | Pluggable event processing and analysis extension layer |
| `src/core/heap_peek/` | Heap observation support |
| `src/core/threading/`, `src/core/trigger/`, `src/core/async/` | Thread capture, trigger waiting, async handoff |
| `src/core/sim_fastforward/` | Simulated fast-forward (sim-skip) for deterministic jumps on the DR backend |
| `src/core/api/` | DLL entry points and C API integration |
| `src/agent/` | In-process Agent IPC, module dump, memory access, session management |
| `src/autostart/` | Autostart helper, config parsing, Agent loading |
| `src/control/` | Shared control-side IPC, injection, and Loader session support |
| `src/tools/vdtrace_ctl/` | Main command-line Agent controller |
| `src/tools/vdtrace_autostart/` | Autostart command-line workflow |
| `src/lite/` | LiteTrace lightweight in-process tracer: INI parsing, step/specified modes, runtime, DllMain |
| `src/tools/examples/` | Example control program and `LiteTrace.ini` sample config |
| `src/plugins/bepinex/` | BepInEx plugin for autostart activation |
| `src/flutter_gui/` | Flutter Windows GUI |
| `src/tests/` | Smoke and workflow regression tests |
| `src/third_party/zydis/` | Embedded Zydis source used by the build |
| `release/LiteTrace/` | LiteTrace release assets: README, config template, Windows build/package scripts |

---

## Build

### Core engine, Agent, tools, and smoke tests

Run from the repository root on Windows with Visual Studio 2022 installed:

```bat
cmd.exe /c "call build_release.bat"
```

Main Release outputs are generated under `bin\release\`.

| Output | Type | Purpose |
|:--|:--|:--|
| `VDTraceStatic.lib` | static library | Core engine for tests and static consumers |
| `VDTrace.dll` | DLL | Core engine dynamic library with exported C API |
| `VDTraceAgent.dll` | DLL | In-process target Agent |
| `VDTraceAutoStart.dll` | DLL | Autostart helper |
| `LiteTrace.dll` | DLL | Lightweight inject-and-go in-process tracer (reads `LiteTrace.ini`) |
| `VDTraceTriggerWaitHelper.dll` | DLL | Trigger/root-stop smoke helper |
| `VDTraceDecryptSmokeHelper.dll` | DLL | Decrypt smoke helper |
| `vdtrace_ctl.exe` | EXE | Agent IPC CLI client |
| `vdtrace_autostart.exe` | EXE | Autostart CLI |
| `vdtrace_example.exe` | EXE | Example controller |
| `vdtrace_*_smoke_test.exe` | EXE | Regression smoke tests |

Run the smoke suite after building:

```bat
cmd.exe /c "cd /d E:\科研\VD-Trace\bin\release && vdtrace_smoke_suite_test.exe"
```

### Flutter GUI

```bat
E:\KDR\flutter\bin\flutter.bat build windows --release
```

Flutter Release output:

```text
src\flutter_gui\build\windows\x64\runner\Release\vdtrace_gui.exe
```

### BepInEx plugin

```bat
dotnet build src\plugins\bepinex\VDTraceAutoStartPlugin.csproj -c Release
```

The plugin output is placed under `bin\release\bepinex_plugin\`.

### LiteTrace lightweight release

One-click build + package (configures CMake, builds only the `LiteTrace` target,
copies artifacts into `dist\`):

```bat
release\LiteTrace\build-and-package.bat
```

This produces `dist\LiteTrace-v<version>\` (with `LiteTrace.dll`, `LiteTrace.ini`,
`README.md`) and a matching `.zip`. You can also run `build_release.bat` for a full
build and then package separately with `release\LiteTrace\package.ps1`. Full usage,
the two run modes (step / specified), and the config reference are in
[`release/LiteTrace/README.md`](release/LiteTrace/README.md).

---

## Verification

Common verification commands:

```bat
cmd.exe /c "call build_release.bat"
cmd.exe /c "cd /d E:\科研\VD-Trace\bin\release && vdtrace_smoke_suite_test.exe"
dotnet build src\plugins\bepinex\VDTraceAutoStartPlugin.csproj -c Release
E:\KDR\flutter\bin\flutter.bat analyze
E:\KDR\flutter\bin\flutter.bat test
```

---

## Current Limitations

| Limitation | Notes |
|:--|:--|
| Single active trace session | One process currently supports one active VDTrace session |
| Indirect calls/jumps | Basic classification is supported, but target reconstruction is not always guaranteed |
| VEH callback cost | Exception-path logic must remain small and predictable |
| Hardware breakpoint count | x86-64 provides only four hardware debug registers; conditional branches can consume two |
| LiteTrace end-point precision | On the DR backend `end_point` matches at control-flow-edge granularity (choose an edge target); use `backend = TF` for instruction-level precision |

---

## Safety and Intended Use

VD-Trace is intended for authorized debugging, diagnostics, runtime research, and reproducible engineering workflows.

The repository is maintained as a general-purpose Windows runtime tracing toolkit. Public defaults avoid target-specific process names, fixed module assumptions, and hardcoded RVAs. Security-sensitive changes such as loader behavior, Agent IPC, memory operations, and autostart paths should be reviewed carefully and covered by regression tests.

---

## Repository Policy

Tracked public source/documentation scope:

- `src/`
- `include/`
- `release/`
- `README.md`
- `README_CN.md`
- `Task-Status.md`
- `.gitignore`
- `.clangd`
- `NuGet.config`
- `CMakeLists.txt`
- `build_release.bat`

Ignored local/generated scope includes build outputs (`bin/`, `obj/`, `dist/`), logs, runtime INI files (except the tracked `LiteTrace.ini` templates), IDE metadata, local index caches, and local application-material drafts.

<div align="center">

**Platform:** Windows x64 | **Toolchain:** Visual Studio 2022 | **Disassembler:** Zydis

</div>
