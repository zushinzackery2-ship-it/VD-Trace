# VD-Trace V2 设计说明

这次改动把库从“只有底层 trace 能力”往“更好用的控制流工具”推了一步，核心目标有三个：

- 同层和浅层流程可以直接配置，不再只能一路钻进所有内部调用
- 默认层级之外，还能按模块 / 匿名执行页 / 模块外区域单独覆写跟进层级
- 首次命中和每次命中两种使用方式可以直接切换
- 跨模块调用可以只记录不进入，业务流程和系统库噪音能直接分开
- 可以先等命中某个地址，再激活正式 Trace
- 可以在命中指定地址时额外抓一小段寄存器/内存值，不要求该地址本身是控制流边
- 可以只看从根入口开始的一次调用子树，返回根层后自动停
- 可以在已知异步投递 API 处自动还原下一跳入口线索
- 日志不只保留原始边，还会补一层更接近 `frida-trace` 的函数调用摘要
- 跨模块 `call/return` 可以按需补一层稀有路径缓冲区前后快照
- C++ API、C API、IPC、CLI、Python GUI 共用同一套配置语义
- 会话页直接补上模块 `Dump+Fix`

## 本次已落地

### 1. 调用层级策略

- `max_call_depth=all`
  保持旧行为，持续跟进内部调用
- `max_call_depth=0`
  只记录当前层 `call` 边，不主动进入 callee
- `max_call_depth=N`
  只跟进 N 层调用，适合看浅层业务流程

控制流模式下，硬件断点后端会根据当前调用深度决定：

- 是否进入下一层调用
- 是否改为等待返回地址
- 是否在返回时回退当前调用深度

### 2. 命中策略

- `first`
  同一条控制流边只输出首次命中，适合摸 CFG
- `every`
  每次命中都输出，适合看循环热点和重复 `call`

`first` 模式下，重复 `jcc` 热循环现在会自动切到热旁路：

- 优先等循环退出边重新落回正式观察
- 当前调用帧的返回点只做兜底
- 这样像 `UnityPlayer` 里 block-size 累加、CRC 这类大循环不会把 `steps` 顶到几百万还卡在同一条边

### 3. 系统模块固定策略

- 命中系统 DLL 调用时，固定只记录 `call` 边，不进入 callee
- 非系统外部模块只有在开启 `trace_outside_modules` 时才会继续跟进
- 关闭 `trace_outside_modules` 时，不管是系统模块还是外部业务模块，都会收敛成“只记边，不主动钻内部”
- 匿名执行页现在单独走匿名页层级规则，不和普通外部模块绑死在一起

### 3.1 追踪层级过滤器

- 默认层级继续由 `max_call_depth` 决定
- 新增 `depth_filter_spec`
  支持：
  - `outside=<层级>`
  - `anon=<层级>`
  - `module=<模块名>:<层级>`
- 这层规则直接作用在 tracer 的 call 跟进决策，不是日志后过滤
- 优先级：
  - 指定模块规则
  - 匿名执行页规则
  - 模块外区域规则
  - 默认 `max_call_depth`

### 4. 调用深度可见性

每条事件新增 `call_depth` 字段，文本日志里会直接输出 `depth=<n>`。

### 5. Recorder 异步化

- 异常路径里只负责把固定大小事件写进预分配环形队列
- 后台线程负责格式化文本和写文件
- 队列满时会丢弃新事件，并在日志里写 `[vdtrace] dropped=<n>`

### 6. 地址触发开始

- `trigger=<0x地址>`
  命中绝对地址后开始
- `trigger=<模块!0xRVA>`
  先按模块基址解析，再等命中该 RVA
- 对 `DR` 和 `TF` 都可用
  真正触发前只挂一个执行断点，不提前进入正式 Trace 状态

### 7. 根调用自动停

- `rootstop`
  从激活点开始，把当前层当成根层
- 当根层出现 `return` 时自动停追
- 适合看 `VFS 末端 -> UnityPlayer` 这类一次性加载链

### 7.1 运行态可见性

`DescribeState()` 和 GUI 状态栏现在会补这些字段：

- `observe`
  当前观察状态，常见值有 `dest / tail / single-step / linear-scan / hot-bypass`
- `watch=[...]`
  当前真正挂上的执行断点地址
- `capture_hits`
  自动线程捕获模式下，触发点一共命中过多少次
- `capture_last`
  自动线程捕获模式下，最近一次命中触发点的线程 ID
- `focus`
  当前线程模式，`single` 表示专注首个线程，`queue` 表示根返回后自动回到等待下一次命中
- `probes`
  当前配置里启用的 probe 点数量
- `hot_streak`
  当前热循环抑制计数
- `hot_resume`
  热旁路正在等的循环退出地址
- `hot_return`
  热旁路兜底等待的调用返回地址

这样实机卡住时可以直接看状态，不用再猜是没进旁路、卡在线性扫描，还是在等返回。

### 8. 异步接力与线索还原

- 后端内置一组常见投递 API：
  `CreateThread`、`CreateRemoteThread(Ex)`、`NtCreateThreadEx`、`_beginthreadex`
  `QueueUserWorkItem`、`RtlQueueWorkItem`
  `TrySubmitThreadpoolCallback`、`CreateThreadpoolWork`
  `QueueUserAPC`、`NtQueueApcThread`
- 真正的线程创建 API 现在会自动切到新线程继续 Trace
- `QueueUserWorkItem` / APC / 线程池当前继续保留“线索还原 + 摘要输出”，不强行自动接力
- 命中这些 API 时，不在 VEH 里做字符串解析和等待，只抓最便宜的参数快照
- 真正的 API 识别、地址标注、摘要拼装和线程接力等待都放到后台线程
- 输出摘要会直接给出线程入口 / callback / APC 入口和上下文参数

### 9. 日志风格补强

- 原始边事件继续保留：
  `rip / rel / target_abs / bytes`
- 跨执行范围时，原始事件行现在会直接补：
  `[JUMP_OUT_OF_TRACE_RANGE] from=... to=...`
  用来显式提示：
  - 模块内跳到别的模块
  - 模块内跳到匿名可执行页
  - 匿名可执行页再跳回模块
- 额外补一层接近 `frida-trace` 的 enter/leave 输出：
  - 普通 `call`：按 `call_depth` 缩进，输出 `callee(arg0=..., arg1=...)`
  - `return`：输出 `<= retval`
  - 已知异步投递 API：输出带命名参数的 `CreateThread(lpStartAddress=..., lpParameter=...)`
- 首次命中的函数入口会在后台线程补一段线性预览反汇编，方便快速确认函数体轮廓
- 对可读且非代码页的参数 / 返回值指针，后台线程会额外补首段 `mem=... ascii="..."` 预览
- 这样既能保留底层可审计性，也能更快扫读调用链

### 10. 稀有路径增强采样

- `enhanced_sampling=false`
  默认关闭，不改原有采样密度
- `enhanced_sampling=true`
  只在跨模块 `call/return` 这种高价值路径上额外抓固定小块缓冲区快照
- 异常路径里只做：
  - 跨模块快判
  - 最多 2 个参数地址的小块内存复制
  - 返回时同地址二次复制
- 真正的 `before/after` 对比和文本输出仍然放后台线程
- 当前输出形态：
  - 进入时：`[sample] arg0@0x... pre=...`
  - 返回时：`[sample] arg0@0x... before=... after=...`
  - 如果返回值本身是可读缓冲区，还会额外输出 `retval@0x... mem=...`

### 11. 定点取值观测

- `probe_spec=""`
  默认关闭，不改原有行为
- `probe_spec="<hit>-><capture>|<capture>;..."`
  用规则文本声明定点取值观测
- `hit`
  支持 `0x地址`、`模块!0xRVA`、`模块+0xRVA`
- `capture`
  当前支持：
  - `reg:rcx[:label]`
  - `mem:0xADDR:size[:label]`
  - `mem:模块!0xRVA:size[:label]`
  - `ptr:rcx+0x10:size[:label]`
- probe 命中后会额外发一条 `kind=probe` 事件，并在文本日志里输出 `[probe]`
- 如果 probe 地址落在当前基本块内部，不是控制流边，当前块会临时切到线性扫描把这个地址抓出来；离开该块后再回到正常 `DR` 观察
- 这样可以直接覆盖：
  - VM 取静态 key
  - 某个块内普通 `mov/lea/xor` 的中间态
  - 栈上临时缓冲区
  - 输出缓冲区地址附近的小片内存

### 12. VEH 热路径继续瘦身

- VEH 入口活动 session 读取已改成原子指针，不再每次异常先抢全局互斥锁
- recorder 唤醒已改成“队列从空变非空 / dropped 从 0 变非 0 时才唤醒”
- 模块范围查找已改成按基址排序后的二分查找
- 停止 trace 时，waiting capture 线程不再同步挨个 `ClearThreadTraceState`
  改成登记为 stale capture，等它下次撞到旧触发点时在 VEH 里就地清掉
  这样能避免 `Stop()` 卡在某个 waiting capture 线程句柄上

## 当前配置入口

### C++ API

- `Options.max_call_depth`
- `Options.depth_filter_spec`
- `Options.hit_policy`
- `Options.trigger_module_name`
- `Options.trigger_address`
- `Options.stop_on_root_return`
- `Options.enhanced_sampling`

### C API

- `VDTRACE_OPTIONS.max_call_depth`
- `VDTRACE_OPTIONS.depth_filter_spec`
- `VDTRACE_OPTIONS.hit_policy`
- `VDTRACE_OPTIONS.trigger_module_name`
- `VDTRACE_OPTIONS.trigger_address`
- `VDTRACE_OPTIONS.stop_on_root_return`
- `VDTRACE_OPTIONS.enhanced_sampling`

### Python GUI

- `层级过滤`
  GUI 里单独拆了一页：
  - 默认层级沿用原数值语义：
    - `0=不限(all)`
    - `1=同层(same)`
    - `2=向下一层`
    - `3=向下两层`
  - 可以额外给模块外区域、匿名执行页和指定模块覆写默认层级
- `触发点`
  支持 `0x地址` 或 `模块!0xRVA`
- `单次调用即停`
  勾上后只看当前根调用子树
- `记录重复命中`
  勾上后切到 `every`
- `指定模块记录`
  勾上后只把模块框里的模块列表当正式记录区域
  取消后允许继续记录外部业务模块
- `TF全量单步（实验）`
  勾上后切到 TF 全量单步后端，干扰更重
- `增强采样（跨模块）`
  勾上后跨模块 `call/return` 会追加缓冲区前后对比
- `自动线程捕获`
  - 有触发点时勾上：自动等待任意线程命中触发点
  - 有触发点时取消：启用手动线程 ID 输入，固定该线程等待触发点
  - 没有触发点时取消：直接追指定线程
  - 没有触发点时勾上：直接追主线程
- `屏蔽主线程`
  - 只在 `定点触发 + 自动线程捕获` 下生效
  - 勾上后主线程命中触发点不会晋升为正式追踪线程
  - session 会继续等待其他线程命中
- `输出`
  GUI 里改成只读展示，不再手填
  按下一键开始后才会自动生成 `.\traces\VDTrace-<pid>-YYYYMMDD-HHMMSS.log`
- `取值观测`
  语法是 `hit->capture|capture`
  `capture` 支持 `reg:rcx[:label]`、`mem:模块!0xRVA:size[:label]`、`ptr:rcx+0x10:size[:label]`
- `会话页 Dump+Fix`
  模块列表和实际镜像复制都在 Agent 内完成，默认输出 `.\dump`
  原始文件后缀是 `_dump_raw`，修正版后缀是 `_dump_fix`
  没拿到真实模块列表时不会回退占位模块名，界面会直接显示失败状态并禁用 Dump

### CLI

- `modules`
  默认枚举指定 PID 的非系统模块文件名
  `modules <pid> all` 可显式查看全量模块
- `dump <pid> <module_name> [output_dir]`
  通过 Agent 内部内存访问同时输出原始镜像和修正后 PE
- `depth=all`
- `depth=same`
- `depth=<数字>`
- `depthfilter=outside=<层级>,anon=<层级>,module=<模块名>:<层级>`
- `hits=first`
- `hits=every`
- `trigger=<0x地址>`
- `trigger=<模块!0xRVA>`
- `probe=<rule[;rule...]>`
- `autothread`
- `blockmain`
- `rootstop`
- `handoff`
- `sample`

### 自动启动器

- 新增 `vdtrace_autostart.exe`
  读取 `vdtrace_autostart.ini`
- 新增 `VDTraceAutoStart.dll`
  由现有 `winhttp.dll` Loader 在线拉起，专门做“等时机 -> 再加载 Agent”
- 当前等待策略：
  - `wait.mode=disabled`
    helper 进入目标进程后立即拉起 `VDTraceAgent.dll`
  - `wait.mode=bepinex_il2cpp_scene_change`
    helper 用 VEH 等 `GameAssembly!il2cpp_runtime_invoke(method)` 命中 `Internal_ActiveSceneChanged`
    命中后才真正 `LoadLibrary` `VDTraceAgent.dll`
- 等待点命中后，helper 会直接按 ini 的 `[trace]` 段发 `configure/start`
- 启动器默认继续等到 trace 结束后退出；如果只想负责拉起，不想等结束，可以把 `launch.wait_for_trace_end=false`

## 下一阶段建议

### 1. 继续做强调用策略

- `只跟进模块内调用`
- `按函数入口白名单决定是否进入`
- `跨模块调用命中后按模块名单二次决策`

### 2. 把异常路径继续做轻

- 用户回调异步分发，避免外部 callback 直接跑在 VEH
- 继续减少异常路径里的字符串和容器操作
- GUI 走独立统计通道，不再高频 tail 文本文件

### 3. 把结果从“日志”升级成“分析结果”

- 热点边计数
- 热点函数计数
- 调用图导出
- JSONL / CSV / Graphviz 导出

### 4. 增加触发条件

- 命中指定函数名后开始
- 返回到指定层级后停止
- 离开模块后停止
- 工作项 / APC / 线程池场景的自动接力

当前这版已经把“线程创建 -> 新线程继续追”这条链做实，日志里也会继续保留下一跳入口 / callback 摘要。

## 当前验证补充

- `vdtrace_agent_smoke_test.exe`
  覆盖 Agent 自举、内部模块枚举、内部 `Dump+Fix`
- `vdtrace_smoke_suite_test.exe`
  统一串行覆盖当前主验证清单：
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
  对 `async_thread_handoff` 做 3 轮专项覆盖
  当前做法是串行拉起 `vdtrace_example.exe --async-handoff`，验证切线程后的退出码、状态文本和稳定性轮次
- `vdtrace_trigger_wait_test.exe`
  覆盖“外部模块触发 -> 目标模块热循环 -> 自动旁路恢复”这条现场形状
- `vdtrace_decrypt_smoke_test.exe`
  覆盖跨模块解密、匿名执行页动态阶段、独立密文落盘、`rootstop`、关键 key schedule 片段输出、参数/返回值缓冲区预览、增强采样前后快照

## 当前已验证的行为边界

- 想完整观察 `模块A -> 模块B -> 模块B 内部算法函数链`，当前稳定配置应开启 `trace_outside_modules`
- 只给 `module_names` 填主模块且关闭 `trace_outside_modules` 时，外部业务 DLL 内部边不会全部进入正式文本输出

## 设计原则

- 默认配置优先稳，不优先“信息最多”
- 控制流模式优先少侵入，不优先万能插桩
- 稀有路径增强采样默认关闭，只在明确高价值路径上做加法
- 新能力优先做成策略，不继续堆零碎布尔开关
