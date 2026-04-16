# VD-Trace

独立的 `DR 主后端 + TF 辅助回退` 控制流 Trace 支持库，主线工作流以 `autostart + BepInEx plugin` 和直接注入为准；`winhttp.dll` 在线会话只保留兼容用途，不再作为默认入口。

当前能力：

- `DR` 硬件观察为主、`TF` 只在必要时补探测
- 指定模块过滤
- 可选整线程追踪
- 运行中 `Start / Stop`
- `max_events`
- `control_flow_only`
- `max_call_depth`（默认层级；`all` / 浅层 / 同层）
- `depth_filter_spec`（按模块 / 匿名执行页 / 模块外区域覆盖默认层级与执行模式）
- `hit_policy`（首次边 / 每次命中）
- `hot_bypass_threshold`（首次命中模式下的空转跳出阈值；默认 32，0 关闭）
- `enhanced_sampling`（跨模块 `call/return` 额外抓缓冲区前后快照）
- `trigger_point`（命中地址后开始 Trace）
- `auto_select_thread`（内核选项；GUI 里勾上“自动线程捕获”且开启“定点触发”时映射到它）
- `block_main_thread`（内核选项；GUI 里勾上“屏蔽主线程”时映射到它）
- `probe_spec`（命中指定地址时额外抓寄存器/内存值，支持块内普通指令地址）
- `stop_on_root_return`（返回根层后自动停）
- 静态引用伴生导出：`*.static_refs.json`
- 线程创建 API 自动跨线程接力（`CreateThread` / `CreateRemoteThread(Ex)` / `_beginthreadex` / `NtCreateThreadEx`）
- 已知异步投递 API 线索还原（`QueueUserWorkItem` / APC / 线程池）
- 原始边日志 + `frida-trace` 风格 enter/leave 输出
- 后台线程首次命中函数预览与反汇编
- C++ API / C API
- 轻量环形队列 + 异步文本文件记录器
- `VDTraceAgent.dll` IPC Agent
- 兼容保留的 `winhttp.dll` Loader 在线会话
- `run_python_cli.bat` 共享配置的 Python CLI 控制端
- `vdtrace_ctl.exe` 原始 IPC CLI 作为低层备用路径
- GUI / CLI 内存读写
- Agent 内部模块 dump + PE 修正输出

当前限制：

- 目前一进程只支持一个活动 Trace 会话
- 间接 `call/jmp` 只做基础分类，不保证总能还原目标地址
- 回调运行在异常处理路径里，回调逻辑必须尽量轻

控制流模式下：

- `max_call_depth=all`：保持旧行为，持续跟进内部调用
- `max_call_depth=0` 或 GUI 填 `single`：只看当前层控制流，内部 `call` 只记边，不主动进入 callee
- `max_call_depth=1/2/...`：只跟进指定层数，适合看浅层业务流程
- `depthfilter=outside=<n>[:edge|tf],anon=<n>[:edge|tf],module=<name>:<n>[:edge|tf]`：给模块外区域、匿名执行页或指定模块单独覆盖默认层级；`TF` 会在该区域局部展开正文
- `hit_policy=first`：同一条边只输出首次命中
- `hit_policy=every`：每次命中都输出，适合看循环热点和重复调用
- `hit_policy=first` 下，重复 `jcc` 热循环会自动切到热旁路：
  优先等循环退出边，函数返回只做兜底，避免 `UnityPlayer` 这类大块累加/校验循环把 `steps` 打爆
- `idleescape=<n>`：首次命中模式下，热点空转累计到阈值后改盯退出边/返回点；默认 32，`0` 关闭
- 系统模块调用固定只记录边，不主动进入内部
- 非系统外部模块只有在开启 `trace_outside_modules` 或显式命中模块层级规则时才会继续跟进；关闭时固定只记边，不主动进入内部
- 匿名可执行页可以按匿名页层级规则继续做最小化跟进，不会因为普通外部模块只记边就一起被关掉
- `trigger=<0x地址>`：命中绝对地址后才正式开始 Trace
- `trigger=<模块!0xRVA>`：命中指定模块内 RVA 后才正式开始 Trace
- `rootstop`：从触发点开始只看这一次调用子树，回到根层 `return` 后自动停

默认 recorder 现在改成轻量环形队列 + 异步落盘：异常路径里只写固定大小事件到预分配队列，后台线程负责格式化、地址标注、异步 API 识别和写文件。高频 Trace 时更稳，但如果写入速度明显追不上采样速度，会在日志里额外写一条 `[vdtrace] dropped=<n>`。

`enhanced_sampling` 默认关闭，只在跨模块 `call/return` 这种稀有路径上额外抓固定小块内存快照。VEH 里只做快判 + 固定字节复制，真正的对比展示仍然放后台线程，所以不会把重格式化逻辑塞回异常处理路径。

线程接力也走后台工作线程：VEH 里只做轻量排队和状态切换，不在异常处理路径里阻塞等新线程起来。

`probe_spec` 也是按这个思路接的：异常路径里只做“命中 probe 地址 -> 拷固定小块值 -> 发事件”，真正的格式化和文本输出仍然在后台线程。

这轮 VEH 热路径还继续收了几刀：

- VEH 入口的活动 session 读取改成原子指针，不再每次异常先抢全局互斥锁
- recorder 只在队列从空变非空时唤醒后台线程，不再每条事件都 `notify_one`
- 模块范围查找改成按基址排序后的二分查找
- 停止 trace 时，不再同步清理所有 waiting capture 线程的 DR；改成登记 stale capture，等它下次撞到旧触发点时在 VEH 里就地清掉，避免 Stop 卡死

日志现在有三层：

- 原始边事件：保留 `rip / rel / bytes / target_abs`
- 跨执行范围提示：离开当前模块 / 进入其他模块 / 进入匿名可执行页时，会追加 `[JUMP_OUT_OF_TRACE_RANGE] from=... to=...`
- `frida-trace` 风格函数行：`callee(arg=...)` 和 `<= retval`
- 参数 / 返回值内存预览：对可读非代码指针追加首段 bytes + ASCII
- 首次命中的函数预览：后台线程按函数入口读取一段连续字节并格式化反汇编
- 对 `.data/.rdata` 静态引用，除了正文里的 `[static]` / `[static.ptr]`，现在还会额外在同目录输出一份 `*.static_refs.json`
  把“静态槽位/指向数据”和“引用它的代码位置”做成映射表

已知异步投递 API 命中时，函数行会直接按命名参数打出线程入口 / callback / APC 入口，适合从 `VFS 末端 -> AB/资源加载` 这类链路里顺手把后续接力函数抠出来。

构建：

```powershell
.\build.bat
```

主要产物：

- `bin\release\VDTraceStatic.lib`
- `bin\release\VDTrace.dll`
- `bin\release\VDTraceAgent.dll`
- `bin\release\VDTraceAutoStart.dll`
- `bin\release\vdtrace_ctl.exe`
- `bin\release\vdtrace_autostart.exe`
- `bin\release\winhttp.dll`
- `bin\release\winhttp_original.dll`

## Python GUI 主流程

1. 把 `bin\release\VDTraceAgent.dll` 放到你方便启动的位置。
2. 运行 `.\run_python_gui.bat`。
3. 启动游戏。
4. GUI 会直接枚举可用目标进程；如果旧 `Loader` 在线，也会一并显示。
5. 选中目标进程后：
   - `加载 Agent`：
     - 有 Loader 时优先发 Loader 请求
     - 没 Loader 时直接走 `vdtrace_ctl.exe inject`
   - `会话页 Dump+Fix`：模块列表和真正的模块镜像复制都走 Agent 内部 IPC，默认输出到 `.\dump`；原始文件后缀是 `_dump_raw`，修正版后缀是 `_dump_fix`。
   - `一键开始`：如果 Agent IPC 还没上线，会先尝试把 `VDTraceAgent.dll` 拉起，再写配置并开始 Trace。
   - `停止`：停止当前 Trace。

默认输出路径是相对路径，例如 `.\traces\VDTrace-<pid>-YYYYMMDD-HHMMSS.log`，会相对 `VDTraceAgent.dll` 所在目录展开，不会强塞到 `C:`；旧的 `.\traces\VDTrace.log` 现在也会在启动前自动升级成带时间戳的新文件名，避免覆盖旧日志。

## Python GUI Loader 兼容流程

如果你还想走旧的 `winhttp.dll` 在线会话链，仍然可以：

1. 把 `bin\release\winhttp.dll` 和 `bin\release\winhttp_original.dll` 放到目标游戏 EXE 同目录。
2. 先运行 `.\run_python_gui.bat`，再启动游戏。
3. GUI 会把旧 Loader 在线会话和直连目标一起显示。

## Python GUI

当前只保留 Python 控制端：

```powershell
.\run_python_gui.bat
```

特点：

- 继续保留分页工作流
- 通过现有 `vdtrace_ctl.exe` 复用 configure/start/stop
- 直接枚举目标进程，同时兼容 Loader 在线会话命名管道
- 使用标准库 `tkinter`，不额外依赖 `PySide6`
- 支持 `触发点`、`自动线程捕获`、`屏蔽主线程`、`单次调用即停`、`异步线程追踪`
- 单独提供 `层级过滤` 分页，支持默认层级、模块外区域、匿名执行页和模块级规则
- 支持 `增强采样（跨模块）`
- 支持 `观测器`
- 会话页直接带 `Dump+Fix`
- GUI / CLI 共用 `vdtrace_gui.ini`
- Trace 预览改成增量刷新，新一轮启动会自动清空旧内容，不再每轮轮询都重写整块文本
- `追踪` 页只保留最近一段预览，避免大日志把 GUI 拖慢；完整内容仍然写到输出文件

Python GUI 和 C++ 核心链路是分离的，只影响控制端体验，不会改到目标进程内的 Trace / Agent / Loader 行为。

## Python CLI

现在还多了一套和 GUI 对齐的 Python CLI：

```powershell
.\run_python_cli.bat config-show
.\run_python_cli.bat sessions
.\run_python_cli.bat load --session-id 1
.\run_python_cli.bat modules --session-id 1
.\run_python_cli.bat dump --session-id 1 --module UnityPlayer.dll
.\run_python_cli.bat start --session-id 1 --thread-capture --modules UnityPlayer.dll --call-depth 3 --outside-call-depth 2 --anonymous-call-depth 2 --trigger UnityPlayer.dll!0x123456 --observer "UnityPlayer.dll!0x123480->mem:UnityPlayer.dll!0x1FE2750:32:keyiv|reg:rsp:rsp"
.\run_python_cli.bat status --session-id 1
.\run_python_cli.bat stop --session-id 1
.\\run_python_cli.bat memory-read --pid 1234 --address "UnityPlayer.dll+0x1FE2770" --size 64
.\\run_python_cli.bat memory-write --pid 1234 --address "0x7ff600001000" --hex "90 90 c3"
```

特点：

- 命令面和 GUI 同语义，不再直接暴露底层 `vdtrace_ctl.exe configure ...` 那套原始 token
- 支持：
  - 在线会话枚举
  - Agent 拉起
  - 模块枚举
  - `Dump+Fix`
  - 启动 / 停止 / 状态
  - `屏蔽主线程`
  - `过滤器`
  - `增强采样`
  - `观测器`
- 默认读取并复用 `vdtrace_gui.ini`
- `config-save` 会直接把 CLI 覆盖后的策略写回共享配置
- `self-test` 可直接跑一轮 CLI 自检：

```powershell
.\run_python_cli.bat self-test
```

## 典型参数

- `自动线程捕获`：
  - 开启 `定点触发` 时勾上：自动等待任意线程命中触发点。
  - 开启 `定点触发` 时取消：启用右侧线程输入框，固定该线程等待触发点，填 `0` 表示主线程。
  - 关闭 `定点触发` 时取消：直接追右侧线程输入框指定的线程，填 `0` 表示主线程。
  - 关闭 `定点触发` 且勾上：直接追主线程。
- `屏蔽主线程`：
  - 只在 `定点触发 + 自动线程捕获` 模式下生效。
  - 勾上后主线程命中触发点不会晋升为正式追踪线程，session 会继续等待其他线程命中。
  - 关闭后保持默认自动捕获行为。
- `模块`：常见场景直接填 `UnityPlayer.dll`。
- `指定模块记录`：勾上后只把模块框里的模块当正式记录区域；取消后允许继续记录到外部业务模块。
- `TF全量单步（实验）`：实验模式，会切到 TF 单步后端，干扰明显更重。
- `层级过滤`：
  - 默认层级仍是 `0=不限(all)`，`1=同层(single)`，`2=向下一层`，`3=向下两层`
  - 现在单独放在 `层级过滤` 分页
  - 可额外给模块外区域、匿名执行页和指定模块覆盖默认层级
  - 每条覆盖规则都能单独选 `EDGE / TF`
  - `TF` 只在该区域局部切入，不会把整条 session 强行退化成全程单步
  - `空转跳出` 默认 32；关掉后不做热点空转逃逸
- `触发点`：支持 `0x地址` 或 `模块!0xRVA`，命中后才正式开始 Trace。
- `输出`：现在改成只读展示，不再手填；按下一键开始后才会自动生成带时间戳的日志名。
- `观测器`：
  `capture` 用 `hit->capture|capture`。
  `step` 用 `step@hit steps=256 exit=return-or-leave`。
  `write` 用 `write@hit watch=addr:size:label|addr:size:label steps=256 exit=return`。
  `capture` 支持 `reg:rcx[:label]`、`mem:0xADDR:size[:label]`、`mem:模块!0xRVA:size[:label]`、`ptr:rcx+0x10:size[:label]`。
  `step/write` 命中后会临时切局部 `TF`；`step` 按指令输出 `kind=probe + [disasm]`，`write` 只在 watch 缓冲区变化时输出 `[probe]` 块。
- `单次调用即停`：适合配合触发点，只看这一次调用子树。
- `异步线程追踪`：命中线程创建 API 后自动切到新线程继续追。
- 系统模块调用现在固定只记录边，不再给单独开关。
- `增强采样（跨模块）`：跨模块 `call/return` 会额外输出参数缓冲区 `before/after` 小快照，适合看解密、解压、协议编解码。
- `记录重复命中`：想看循环体和重复 `call` 的真实命中次数时勾上。
- `Trace 预览`：现在除了原始边事件，还会多出 enter/leave 风格函数行和首次命中函数预览。
- `参数 / 返回值预览`：对可读缓冲区参数和值会自动补 `mem=... ascii="..."`，更适合看解密、解压、密钥表。
- `运行状态`：状态栏现在会直接给出 `observe`、`watch`、`hot_streak`、`hot_resume`、`hot_return`，卡现场时不用再靠猜测。
- `线程状态`：状态栏会额外显示 `线程模式=`、`活动线程=`；自动捕获线程等待中还会带 `捕获=thread`，命中过触发点后还会补 `触发命中=` 和 `最近命中线程=`。
- `观测器状态`：状态栏会额外显示 `观测器=`，直接看当前配置里启用了几条规则。
- `内存页`：支持简单内存读写；地址支持 `0xADDR` / `module+0xRVA` / `module!0xRVA`，追踪页和日志页都能把选中地址一键带过去。
- `会话页 Dump+Fix`：模块列表直接来自目标进程模块快照，真正的 dump/fix 走 Agent 内部 IPC，默认把原始镜像和修正后 PE 输出到 `.\dump`。
- `真实模块列表`：没拿到真实枚举结果时不会再回退假模块名，界面会直接显示枚举失败或为空，并禁用 Dump。
- `系统模块过滤`：GUI 模块列表和 `vdtrace_ctl.exe modules <pid>` 默认都会过滤系统模块；如果真要看全量，再用 `vdtrace_ctl.exe modules <pid> all`。
- `Trace 预览`：当前轮次不再硬截到最近 `2048` 行，会继续完整追加；日志太长时直接看输出文件更合适。

## 底层 CLI 备用路径

如果你不走 `winhttp` 劫持，也可以直接用 CLI：

```powershell
.\bin\release\vdtrace_ctl.exe inject <pid> .\bin\release\VDTraceAgent.dll
.\bin\release\vdtrace_ctl.exe modules <pid>
.\bin\release\vdtrace_ctl.exe modules <pid> all
.\bin\release\vdtrace_ctl.exe dump <pid> UnityPlayer.dll .\dump
.\\bin\\release\\vdtrace_ctl.exe read <pid> UnityPlayer.dll+0x1FE2770 64
.\\bin\\release\\vdtrace_ctl.exe write <pid> 0x7ff600001000 9090c3
.\bin\release\vdtrace_ctl.exe configure <pid> <thread_id> UnityPlayer.dll .\traces\VDTrace-1234-20260406-023000.log 2048 depth=single "depthfilter=outside=2:edge,anon=2:tf,module=GameAssembly.dll:all:tf" hits=every idleescape=32 sample autothread blockmain trigger=UnityPlayer.dll!0x123456 probe=UnityPlayer.dll!0x123480->mem:UnityPlayer.dll!0x1FE2750:32:keyiv|reg:rsp:rsp rootstop
.\bin\release\vdtrace_ctl.exe start <pid>
.\bin\release\vdtrace_ctl.exe stop <pid>
```

## 自动启动器

如果你想做“启动游戏后，先等运行时就位，再自动拉起 tracer 并按 ini 直接开追”，现在有单独的自动启动链：

```powershell
.\bin\release\vdtrace_autostart.exe
```

默认读取仓库根目录的 `vdtrace_autostart.ini`。

当前这条链的行为是：

- 启动目标游戏
- 部署 `BepInEx\plugins\VDTraceAutoStartPlugin.dll`
- 写入 `VDTraceAutoStart.activate.ini`
- 由 `BepInEx` 插件在目标进程里加载 `VDTraceAutoStart.dll`
- `VDTraceAutoStart.dll` 用 VEH 等 `GameAssembly!il2cpp_runtime_invoke(method)` 命中 `Internal_ActiveSceneChanged`
- 命中后再 `LoadLibrary` `VDTraceAgent.dll`
- 按 ini 里的 trace 配置自动 `configure/start`
- 启动器默认继续等待 trace 结束，然后退出

当前边界：

- 只支持 IL2CPP / BepInEx 风格的 `Internal_ActiveSceneChanged` 等待点
- 不再依赖 `winhttp.dll` Loader 先上线
- 真正 trace 的配置仍然走 ini 里的 `[trace]` 段，和 GUI 语义一致
- `call_depth` 仍是默认层级；`outside_call_depth` / `anonymous_exec_call_depth` / `module_call_depths` 可以额外覆盖
- `outside_execution_mode` / `anonymous_exec_execution_mode` 支持 `EDGE / TF`
- `module_call_depths` 现在支持 `ModuleA.dll:3:TF,ModuleB.dll:1:EDGE`
- `idle_escape_threshold` 控制首次命中模式下的空转跳出阈值；默认 32，0 关闭
- helper 诊断日志默认输出到 `.\traces\VDTraceAutoStart-时间戳.log`

## 冒烟测试

新增两套本地 smoke：

- `vdtrace_agent_smoke_test.exe`
  自举 `VDTraceAgent.dll`，验证 Agent 内部模块枚举、内部 `Dump+Fix` 和输出文件落盘
- `vdtrace_smoke_suite_test.exe`
  串行拉起整套核心 smoke，当前会覆盖：
  `vdtrace_example`
  `vdtrace_example --async-handoff`
  `vdtrace_async_handoff_smoke_test`
  `vdtrace_agent_smoke_test`
  `vdtrace_session_smoke_test`
  `vdtrace_trigger_wait_test`
  `vdtrace_rootstop_test`
  `vdtrace_stop_recovery_test`
  `vdtrace_decrypt_smoke_test`
- `vdtrace_session_smoke_test.exe`
  默认只跑问题点快回归：
  `same-level`
  `outside-depth-filter`
  `anonymous-depth-filter`
  `heap-extend`
  `static-refs`
  `hot-loop-bypass`
  `auto-thread-capture`
  需要完整 core 时用 `--full-core`
  需要专项复现时用：
  `--case <name>`
  `--case stability-rounds --stability-case <name> --rounds <n>`
- `vdtrace_async_handoff_smoke_test.exe`
  对 `async_thread_handoff` 做 3 轮专项回归，串行拉起 `vdtrace_example.exe --async-handoff`，断言切线程和退出码都稳定
- `vdtrace_trigger_wait_test.exe`
  验证“外部模块触发 -> 目标模块正式开始 Trace -> 目标模块热循环自动旁路”这条实战形状
- `vdtrace_decrypt_smoke_test.exe`
  生成独立密文文件，跨模块进入解密 helper，再跳入匿名执行页执行动态生成的解密阶段；验证 `rootstop`、算法主链、动态页进出标记、解密结果和增强采样前后快照

其中 `vdtrace_decrypt_smoke_test.exe` 会写固定测试产物名，跑回归时应串行执行。

想把“主模块 -> 外部业务 DLL -> 该 DLL 内部算法函数”这一整条链完整打出来，当前稳定做法是开启 `trace_outside_modules`；否则外部模块内部边默认不会全部进入正式输出面。
