# LiteTrace

**轻量版 VD-Trace 进程内追踪器（Windows x64）**

LiteTrace 是一个把 VD-Trace 追踪核心打包成"注入即用"的单 DLL。注入目标进程后，它按**相对路径**读取 `LiteTrace.ini`，在配置的 `trigger_point` 处等待触发，然后记录控制流 trace，跑完后可选择结束进程。全程不需要 Agent、命名管道 IPC、也不需要启动器拉起游戏。

---

## 1. LiteTrace 是什么

- **单文件、进程内**：只有一个 `LiteTrace.dll`，注入后自己读配置、自己 trace，产物是一份文本 trace 日志。
- **核心复用，不重写**：直接静态链接 VD-Trace 核心（`VDTraceStatic`），使用同一套 `Session` / `Options` API、同一套 DR 硬件断点 + TF 局部回退后端、同一套环形队列异步落盘。追踪能力、事件格式和 VD-Trace 一致。
- **纯断点触发**：谁先执行到 `trigger_point`，就追谁那条线程。没有线程指定、线程轮转、屏蔽主线程、跨线程接力这些复杂线程逻辑（那些属于 AutoStart / Agent）。
- **两种结束方式**：按步数（step 模式）或按结束点（specified 模式）停止，见下文。

适用场景：你已经知道要从哪个地址开始追、追多少步 / 追到哪里为止，只想快速拿到一段控制流 trace，而不想搭 GUI / CLI / Agent IPC 那一整套。

---

## 2. 与 VDTrace / AutoStart 的区别

| 维度 | LiteTrace | VDTraceAgent（GUI / CLI 控制） | VDTraceAutoStart |
|:--|:--|:--|:--|
| 交付形态 | 单个 `LiteTrace.dll` | `VDTraceAgent.dll` + `vdtrace_ctl.exe` / Flutter GUI | `VDTraceAutoStart.dll` + `vdtrace_autostart.exe` |
| 控制方式 | 读 `LiteTrace.ini`，无外部控制 | 命名管道 IPC（configure / start / stop / dump / 内存读写…） | 激活文件 + INI，bootstrap → configure → start |
| 是否需要 IPC | 否 | 是（`\\.\pipe\VDTrace-<PID>`） | 是（内部调用 Agent IPC） |
| 是否拉起进程 | 否（注入到已运行进程） | 否 | 是（`[launch]` 段可启动目标） |
| 线程模型 | 固定："当前命中线程"，无线程选项 | 完整：自动捕获 / 轮转 / 屏蔽主线程 / 异步接力 | 完整（复用 Agent） |
| 结束条件 | step 步数 / specified 结束点 | 由控制端 stop，或 `max_events` | 由 `[wait]` / `max_events` / root 返回 |
| 追踪核心 | 与 VD-Trace **完全相同**（`VDTraceStatic`） | 相同 | 相同 |
| 适合 | 快速、最小依赖的一次性 trace | 交互式诊断、Dump+Fix、内存读写 | IL2CPP/BepInEx 场景自动拉起并追踪 |

一句话：**追踪能力都一样，LiteTrace 去掉的是"控制面"（IPC、线程编排、进程拉起），只留"注入 → 断点 → trace"。**

---

## 3. 发布包内容

```
LiteTrace-v<version>/
    LiteTrace.dll      注入用的追踪 DLL（Windows 上构建产出）
    LiteTrace.ini      配置模板（就地修改即可用）
    README.md          本说明
    VDTrace.dll        （可选）核心动态库，随手带上便于对照，LiteTrace 已静态链接核心
```

源码仓库里的 `release/LiteTrace/` 只包含**文档、配置模板和构建/打包脚本**；真正的 `LiteTrace.dll` 由 Windows 构建脚本生成到 `dist/LiteTrace-v<version>/`（见第 8 节）。

---

## 4. 快速开始

1. **准备配置**：把 `LiteTrace.ini` 放到 `LiteTrace.dll` 同目录，或目标进程的工作目录。改好 `modules` 和 `trigger_point`。
2. **注入**：用任意 DLL 注入器把 `LiteTrace.dll` 注入目标进程（例如 `vdtrace_ctl.exe inject`、注入器工具，或 BepInEx / 第三方 loader）。
3. **触发**：目标执行到 `trigger_point` 时，trace 自动开始。
4. **取结果**：trace 写到 `output_path`（默认 `.\traces\LiteTrace.log`，相对 `LiteTrace.ini` 目录）。运行诊断日志写到 DLL 目录下的 `traces\LiteTrace-<PID>.log`。

**配置查找顺序**：当前工作目录 → `LiteTrace.dll` 所在目录。若两处都没有 `LiteTrace.ini`，LiteTrace 会在该位置写出一份默认模板再退出解析（方便你直接编辑）。

---

## 5. 两种模式

在 `[lite]` 段用 `mode` 选择：

### Step 模式（`mode = step`，默认）

从 `trigger_point` 开始，记录 `max_events` 步后停止。**结束点相关配置（`end_point`、`root_stop_on_return`）被忽略。**

```ini
[trace]
modules = GameAssembly.dll
trigger_point = GameAssembly.dll+0x1234
max_events = 20000

[lite]
mode = step
exit_process_on_finish = false
```

用途：只想看"从这里开始的前 N 步在干嘛"。

### Specified 模式（`mode = specified`）

从 `trigger_point` 开始，一直追到执行**到达 `end_point`** 为止。`max_events` 被强制置 0（无限步数），**步数配置被忽略。**

```ini
[trace]
modules = GameAssembly.dll
trigger_point = GameAssembly.dll+0x1234
end_point = GameAssembly.dll+0x5678

[lite]
mode = specified
exit_process_on_finish = false
```

用途：想看"从 A 到 B 之间走了哪些控制流"。

> specified 模式必须提供 `end_point`（或显式打开 `root_stop_on_return`），否则 LiteTrace 会拒绝启动并在诊断日志里报错，避免无限 trace 把磁盘写满。

---

## 6. trigger_point / end_point 地址格式

两个键格式相同，三选一：

| 写法 | 含义 |
|:--|:--|
| `Module.dll+0xRVA` | 模块基址 + RVA 偏移（推荐，抗 ASLR） |
| `Module.dll!0xRVA` | 同上，`!` 分隔 |
| `0xABSOLUTE` | 绝对虚拟地址（不推荐，随 ASLR 变化） |

`end_point` 建议选一个真正的**控制流边目标**（函数入口、call/jmp 目标、分支目标）——这也是"结束点"最自然的选法，原因见第 8 节精度说明。

---

## 7. 配置项参考

### `[trace]` 段（追踪范围与后端，来自 VD-Trace `Options`）

| 键 | 默认 | 说明 |
|:--|:--|:--|
| `modules` | 空 | 要追踪的模块，逗号分隔；空 = 追踪进程主模块 |
| `output_path` | `.\traces\LiteTrace.log` | trace 输出路径；相对路径基于 `LiteTrace.ini` 目录 |
| `trigger_point` | 空 | 起点地址，命中后开始 trace（格式见第 6 节） |
| `trigger_enabled` | `true` | 关掉则忽略 `trigger_point`，注入后立即开始 |
| `end_point` | 空 | 终点地址（**仅 specified 模式**用），到达即停止 |
| `probe_spec` | 空 | 探针规格（capture / step / write），语法同 VD-Trace |
| `max_events` | `0` | 步数（**仅 step 模式**用）；specified 模式强制为 0 |
| `backend` | `DR` | `DR`（硬件断点，控制流边）或 `TF`（全量单步） |
| `all_events` | `false` | `true` 等价于 `backend = TF` 全量 trace |
| `idle_escape_threshold` | `32` | 热旁路阈值，热点循环重复超过此次数后 DR 重编程到出口 |
| `call_depth` | `4` | 追踪调用深度：`single` / `all` / 数字 |
| `outside_call_depth` | 空 | 模块外区域独立深度 |
| `outside_execution_mode` | `EDGE` | 模块外执行模式：`EDGE` / `TF` |
| `anonymous_exec_call_depth` | 空 | 匿名可执行页（JIT）独立深度 |
| `anonymous_exec_execution_mode` | `EDGE` | 匿名页执行模式：`EDGE` / `TF` |
| `module_call_depths` | 空 | 指定模块独立规则，`Module.dll:3:TF`，逗号分隔 |
| `trace_outside_modules` | `false` | 是否记录追踪模块之外的执行 |
| `repeat_hits` | `false` | `false` = 每条边只记首次；`true` = 每次命中都记 |
| `sim_fast_forward` | `false` | DR 后端：确定性直接跳转用模拟快进跳过单步异常 |
| `sim_fast_forward_indirect` | `false` | 额外解析纯寄存器计算的间接跳转（建议 Windows 验证后再开） |
| `enhanced_sampling` | `false` | 跨模块 call/return 抓参数缓冲区 before/after 快照 |
| `root_stop_on_return` | `false` | 根调用帧返回时停止（specified 模式可作为额外结束条件） |

### `[lite]` 段（LiteTrace 运行行为）

| 键 | 默认 | 说明 |
|:--|:--|:--|
| `mode` | `step` | `step`（trigger + max_events）或 `specified`（trigger + end_point） |
| `exit_process_on_finish` | `false` | trace 停止后是否 `ExitProcess` 结束目标进程 |

> LiteTrace **刻意没有**这些键：`thread_id` / `auto_select_thread` / `block_main_thread` / `async_thread_handoff`（线程编排）、`finish_timeout_ms` / `poll_interval_ms`（超时）、`[launch]` / `[wait]`（拉起 / 等待）。它们属于 AutoStart / Agent。旧 INI 里若残留这些键会被直接忽略，不影响运行。

---

## 8. 注意事项

- **DR vs TF 结束点精度**：
  - `backend = DR`（默认）按**控制流边**匹配结束点：当某条被记录的边"到达" `end_point` 时停止。所以 `end_point` 要选真正的边目标（函数入口 / call / jmp / 分支目标）。若把 `end_point` 设在一个基本块中间、永远不会作为边目标出现的地址，DR 模式可能追不到它。
  - `backend = TF`（全量单步）按**任意指令地址**精确匹配，可停在任意一条指令处，代价是全程单步、开销大得多。需要落在任意指令上的精确结束点时用 TF。
- **停止语义**：DR 模式在"记录了到达结束点的那条边之后"停止；TF 模式在"即将执行结束点那条指令之前"停止。两者最后一条记录都是"进入结束点的那次转移"，结束点本身不算已执行。
- **线程语义固定**：只追第一个命中 `trigger_point` 的线程，不追它 spawn 的新线程、不轮转、不屏蔽主线程。需要这些请用 AutoStart / Agent。
- **没有超时**：停止条件只有 step 步数到、specified 结束点到（或可选 root 返回）、或会话自然结束。不会因为"等太久"而中途停。
- **路径是相对的**：`output_path` 相对 `LiteTrace.ini` 目录解析；诊断日志固定在 DLL 目录的 `traces\` 下。确保进程对这些目录有写权限。
- **注入时机**：要在目标执行到 `trigger_point` **之前**注入，否则会错过触发。
- **`exit_process_on_finish`**：发布模板默认 `false`（更安全，trace 完不强杀进程）；示例里可按需改 `true`。

---

## 9. 在 Windows 上构建真正的 Release

追踪核心是 Windows 专有（VEH + 硬件调试寄存器），无法在 Linux 容器里编译，必须在 Windows + Visual Studio 2022（x64）上构建。

### 一键构建 + 打包（推荐）

```bat
release\LiteTrace\build-and-package.bat
```

该脚本会：
1. 用 `vswhere` 定位 VS2022（找不到则回退到脚本里的 `VSPATH`，可自行修改）；
2. CMake 配置 + 只构建 `LiteTrace` 目标（会自动带上依赖 `VDTraceStatic`）；
3. 调 `package.ps1` 把产物打包到 `dist\LiteTrace-v<version>\` 并生成同名 `.zip`。

产出：

```
dist\LiteTrace-v0.1.0\
    LiteTrace.dll
    LiteTrace.ini
    README.md
dist\LiteTrace-v0.1.0.zip
```

### 手动构建

```bat
:: 仓库根目录，构建全部产物（含 LiteTrace.dll）
call build_release.bat
:: 或只构建 LiteTrace 目标
cmake -S . -B obj\cmake-x64-release -G "Visual Studio 17 2022" -A x64
cmake --build obj\cmake-x64-release --config Release --target LiteTrace --parallel
```

`LiteTrace.dll` 会输出到 `bin\release\LiteTrace.dll`。之后可单独跑打包脚本：

```bat
powershell -ExecutionPolicy Bypass -File release\LiteTrace\package.ps1 -Version 0.1.0
```

---

## 10. 已知限制

- 追踪核心仅 Windows x64；不能在无 MSVC 的环境里编译（本分支的 Linux CI 只做语法/结构自检）。
- 单进程单会话：一个进程同时只有一个活动 trace。
- 间接 call/jmp 只做基础分类，目标地址不保证总能还原。
- DR 模式的 `end_point` 按控制流边粒度匹配（见第 8 节）；要指令级精度用 TF。
- VEH 回调运行在异常路径上，逻辑必须轻量。
- x86-64 只有 4 个硬件调试寄存器，条件跳转会占用 2 个。

---

## English (brief)

**LiteTrace** packages the VD-Trace tracing core into a single inject-and-go DLL.
After injection it reads `LiteTrace.ini` by relative path (current working
directory first, then the DLL folder), waits at the configured `trigger_point`,
records a control-flow trace, and can optionally end the process.

It statically links the same `VDTraceStatic` core as VD-Trace, so tracing power,
DR/TF backends and output format are identical. Unlike `VDTraceAgent` (IPC/GUI/CLI
control) and `VDTraceAutoStart` (process launch + Agent IPC), LiteTrace drops the
whole control plane: no IPC, no thread orchestration, no process launching. It is
a pure trigger tracer — whichever thread reaches `trigger_point` is traced.

**Two modes** (`[lite] mode`):

- `step` (default): `trigger_point` + `max_events`; end-point keys are ignored.
- `specified`: `trigger_point` + `end_point`; `max_events` is forced to 0
  (unlimited). A `specified` config with no `end_point` (and no
  `root_stop_on_return`) is rejected so it can never trace unbounded.

`trigger_point` / `end_point` accept `Module.dll+0xRVA`, `Module.dll!0xRVA`, or
`0xABSOLUTE`. In DR mode the end point matches at control-flow-edge granularity
(pick a real edge target); in TF mode (`backend = TF`) it matches any instruction
address exactly.

**Build (Windows, VS2022 x64):** run `release\LiteTrace\build-and-package.bat` for
a one-click configure + build + package into `dist\LiteTrace-v<version>\` (plus a
zip), or `build_release.bat` then copy `bin\release\LiteTrace.dll`. The core is
Windows-only (VEH + hardware debug registers) and cannot be built on Linux.
