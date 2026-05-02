<div align="center">

# VD-Trace

**Windows x64 Runtime Control-Flow Trace Framework**

*Hardware breakpoint-driven tracing | DR primary + TF fallback | Ring-buffer async recorder*

![C++](https://img.shields.io/badge/C%2B%2B-20-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey?style=flat-square)
![Toolchain](https://img.shields.io/badge/Toolchain-Visual%20Studio%202022-green?style=flat-square)
![Disasm](https://img.shields.io/badge/Disassembler-Zydis-orange?style=flat-square)

</div>

---

> [!NOTE]
> **当前定位**
> VD-Trace 以硬件调试寄存器（DR0-DR3）为主后端，在基本块粒度上捕获控制流边；仅在匿名执行页、深度过滤 TF 区域或探针单步场景下临时切入 Trap Flag 模式。
> 主线工作流为 `autostart + BepInEx plugin` 直接注入；`winhttp.dll` 在线会话仅保留兼容用途。

---

## 功能概览

| 功能 | 说明 |
|:-----|:-----|
| **DR 基本块追踪** | 在基本块尾部的控制流指令处设置硬件执行断点，每个基本块仅触发一次 VEH 异常 |
| **TF 局部回退** | 匿名页 / 深度过滤 TF 区域 / 探针 step 模式下临时切入单步，退出后自动恢复 DR |
| **深度过滤** | `depth_filter_spec` 支持模块外、匿名执行页、指定模块三级独立层级与执行模式覆盖 |
| **热旁路逃逸** | `hot_bypass_threshold` 识别热点循环后 DR 重编程至退出点，避免日志爆炸 |
| **触发点** | `trigger_point` 支持绝对地址 / `module!RVA`，命中后才正式开始追踪 |
| **线程自动捕获** | 所有线程同时挂载 DR，首个命中线程通过原子竞争晋升为追踪目标 |
| **异步线程接力** | 检测 `CreateThread` / `_beginthreadex` 等 API 后自动切换到新线程继续追踪 |
| **探针观测** | `probe_spec` 支持 Capture（截获寄存器/内存）、Step（TF 步进）、Write（缓冲区变化监控）三种模式 |
| **增强采样** | 跨模块 call/return 时自动抓取参数缓冲区 before/after 快照并 diff 输出 |
| **静态引用分析** | 基本块内 RIP-relative 内存引用自动解析为 `.data/.rdata` 槽位映射，伴生 `*.static_refs.json` |
| **环形队列 + 异步落盘** | 65 万级预分配 Ring Buffer + Worker 格式化线程 + Writer 写盘线程，三级流水线背压控制 |
| **IPC Agent** | `VDTraceAgent.dll` 注入目标进程后通过命名管道提供 configure / start / stop / modules / dump / memory R/W 服务 |
| **运行时 PE 修正** | Agent 内置 Dump+Fix，修正节表偏移并清除无效 Security 目录，输出可被 IDA 直接加载的 PE |
| **frida-trace 风格输出** | 缩进 call/return 调用树 + 参数/返回值内存预览 + 首次命中函数反汇编预览 |

---

## 核心架构

### 异常驱动模型 (VEH Pipeline)

系统完全构建于 Windows VEH（Vectored Exception Handling）之上：

- **VEH 入口**：`AddVectoredExceptionHandler(1, ...)` 注册最高优先级 handler，分发 `EXCEPTION_SINGLE_STEP` 异常
- **无锁 Session 定位**：`std::atomic<Session::Impl*> g_active_impl` 原子指针，VEH 热路径零 mutex
- **指令分类**：Zydis 反汇编引擎将指令映射为 Call / Jump / ConditionalJump / Return / Syscall / Interrupt 六类控制流节点
- **模块二分查找**：`module_ranges` 按基址排序后 `std::upper_bound` O(log n) 定位
- **线程本地 region 缓存**：`thread_local VirtualQuery` 缓存避免热路径内核调用

### 双模后端

| 模式 | 触发条件 | DR 编程策略 |
|:-----|:---------|:------------|
| **DR (主力)** | 模块内正常追踪 | 条件跳转 → Dr0=target, Dr1=fallthrough；直接 call/jmp → Dr0=target；间接 → Dr0=tail 单步一次 |
| **TF (辅助)** | 匿名页 / 深度过滤 TF 规则 / 探针 step/write / 等待间接目标 | 设置 EFlags.TF，每条指令触发异常，退出区域后 `RestoreHardwareFlowAfterTrapWindow()` 恢复 DR |

### 三级流水线异步 I/O

```text
VEH Handler (被追踪线程上下文)
    │ callback(event) → Enqueue()
    ▼
[Stage 1] Ring Buffer — 655360 条预分配 StepEvent，队列满时 producer_cv.wait() 阻塞
    │ worker_cv.notify_one() (仅从空→非空时唤醒)
    ▼
[Stage 2] WorkerLoop — 批量 drain，格式化 / 地址标注 / API 识别 / 函数预览 / static_refs 分析
    │ EnqueueWrite() (16MB 背压上限)
    ▼
[Stage 3] WriterLoop — WriteFile() 落盘
```

### 深度过滤系统

```text
depthfilter=outside=2:edge,anon=all:tf,module=GameAssembly.dll:all:tf
```

| 规则类型 | 语法 | 语义 |
|:---------|:-----|:-----|
| 模块外 | `outside=<depth>[:edge\|tf]` | PE 映像但非追踪模块的代码区域 |
| 匿名页 | `anon=<depth>[:edge\|tf]` | 非 PE 映像的可执行内存（JIT 代码） |
| 指定模块 | `module=<name>:<depth>[:edge\|tf]` | 为特定 DLL 定义独立层级与执行模式 |

`edge` 模式使用 DR 硬件断点；`tf` 模式在该区域局部切入 Trap Flag 单步，退出区域后自动恢复 DR。

### 热旁路 (Hot Bypass)

当 `hit_policy=first` 下同一条件跳转边被重复命中超过 `hot_bypass_threshold`（默认 32）次时：
1. 计算循环退出地址（fallthrough 或跳转对端）
2. 将 DR 重编程至退出点
3. 进入 `WaitingForHotReturn` 沉睡态
4. 循环结束后恢复正常追踪

### 探针规格 (probe_spec)

三种观测模式，分号分隔多条规则：

| 模式 | 语法 | 运行时行为 |
|:-----|:-----|:-----------|
| **Capture** | `hit->reg:rcx\|mem:0xADDR:size[:label]` | VEH 中同步读取 CONTEXT 寄存器 / 固定地址内存 |
| **Step** | `step@hit steps=N exit=return\|leave\|return-or-leave` | 命中后临时切入 TF，每步发射 Probe 事件 |
| **Write** | `write@hit watch=addr:size[:label]\|... steps=N exit=...` | TF 单步 + 每步 memcmp watch 目标，仅变化时输出 |

支持 `reg:rcx`、`mem:module!0xRVA:32`、`ptr:rcx+0x10:32` 三种操作数格式。

### 线程模型

- **自动捕获**：`BeginTriggerThreadCapture()` 枚举全部线程，在触发地址挂 DR；`StartTriggerCaptureRefreshWorker()` 每 10ms 刷新新线程
- **原子竞争**：首个命中线程通过 `compare_exchange_strong` 从 0 晋升为 `active_thread_id`
- **block_main_thread**：主线程加入 `known_thread_ids` 但不挂断点，自然跳过
- **异步接力**：命中 `CreateThread` 后解析入口参数 → 后台 Worker 轮询新线程就绪 → 原子切换追踪上下文
- **排队模式**：`queue_trigger_threads` 启用时后续命中线程被 park，当前追踪结束后 `RotateQueuedTriggerTrace()` 切换

### IPC Agent 协议

| 字段 | 说明 |
|:-----|:-----|
| 管道名 | `\\.\pipe\VDTrace-<PID>` |
| 协议版本 | `kIpcVersion = 17` |
| 通信模式 | 请求-响应，消息模式 (`PIPE_TYPE_MESSAGE`) |
| 请求结构 | `IpcCommand` (~16KB)：version + type + payload union |
| 响应结构 | `IpcResponse`：version + status + message[16384] |

支持命令：Ping / Configure / Start / Stop / Status / ListModules / DumpModule / ReadMemory / WriteMemory / Shutdown

---

## 源码结构

| 模块 | 产物 / 职责 |
|:-----|:------------|
| `src/VDTrace*.cpp` | 核心追踪引擎：VEH 管线、DR/TF 后端、指令解码、深度过滤、探针、增强采样、静态引用、环形队列 recorder |
| `src/agent/VDTraceAgent*.cpp` | Agent DLL：IPC 服务、Session 管理、模块 Dump+Fix、内存读写 |
| `src/autostart/VDTraceAutoStart*.cpp` | 自动启动 Helper：INI 解析、il2cpp VEH 断点等待、Agent 加载与 configure/start |
| `src/tools/vdtrace_ctl*.cpp` | IPC CLI 客户端：inject / configure / start / stop / modules / dump / read / write |
| `src/tools/vdtrace_autostart*.cpp` | 自动启动器 CLI：插件部署、游戏启动、等待 trace 完成 |
| `src/tools/VDTraceControlSupport*.cpp` | 控制端共享层：命名管道通信、DLL 注入、Loader 会话 |
| `src/python_gui/` | Python GUI (tkinter) 与 CLI 控制端，共享配置模型 |
| `src/tests/` | Smoke 回归测试套件 |
| `include/VDTrace/` | 公共 API 头文件 (`VDTrace.h`, `VDTraceC.h`, `VDTraceIpc.h`) |
| `include/third_party/zydis/` | Zydis 反汇编引擎头文件 |

---

## 构建

```bat
build.bat
```

产物输出到 `bin\release\`，中间文件输出到 `obj\`。

| 产物 | 类型 | 说明 |
|:-----|:-----|:-----|
| `VDTraceStatic.lib` | 静态库 | 核心引擎，供测试 exe 链接 |
| `VDTrace.dll` | DLL | 核心引擎动态库版本（导出 C API） |
| `VDTraceAgent.dll` | DLL | 注入目标进程的追踪代理 |
| `VDTraceAutoStart.dll` | DLL | 自动启动 Helper |
| `vdtrace_ctl.exe` | EXE | IPC 命令行客户端 |
| `vdtrace_autostart.exe` | EXE | 自动启动器 |

---

## 当前限制

| 限制 | 说明 |
|:-----|:-----|
| 单会话 | 一进程只支持一个活动 Trace 会话 |
| 间接 call/jmp | 只做基础分类，不保证总能还原目标地址 |
| VEH 回调开销 | 回调运行在异常处理路径里，逻辑必须尽量轻 |
| 硬件断点数量 | x86-64 仅 4 个 DR 寄存器，条件跳转需占用 2 个 |

---

## 仓库提交规则

- 提交范围：`src/`、`include/`、`README.md`、`.gitignore`
- 忽略范围：`bin/`、`obj/`、`docs/`、`tools/`、`ref_pic/`、`backup/`、`*.ini`、`*.log`、`*.bat`、测试产物、构建中间文件

<div align="center">

**Platform:** Windows x64 | **Toolchain:** Visual Studio 2022 | **Disassembler:** Zydis

</div>
