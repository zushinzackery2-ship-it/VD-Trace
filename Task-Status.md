# Task-Status

## 1. 当前任务 / 需求 / 待办清单

- 当前无未完成交付项。

## 2. 已解决问题 / 已完成需求

- 已清理单游戏专项入口与 WinHTTP 历史命名：Loader IPC 统一为 `VDTraceLoaderControl`，移除专项 proxy 构建/源码/BepInEx 进程绑定/恢复逻辑，默认 trace/profile 不再写死固定目标 RVA。
- 已将 2026-05-09 提交 `7f02012` 导出到 `ref_pic/vdtrace_2026-05-09_7f02012/` 作为老版本参考。
- 已确认 2026-05-09 老版本与当前版本核心 Win32 API 使用面基本一致：ToolHelp32、OpenThread、Get/SetThreadContext、VEH、VirtualQuery/Protect、注入链均沿用同类机制；当前主要是目录重构和控制端迁移。
- 已按老版本 Python/Tk GUI 的稳定 Loader pipe 交互修当前 Flutter GUI：恢复长连接会话句柄、按 C++ header.size 协议读整包、修正 AgentHello 字段偏移、支持发送 LoadDllRequest、记录 LoadDllReply。
- 已把 Flutter 控制器改成老版本 Python/Tk GUI 的自动 IPC 会话工作流：自动选中首个 Loader 会话；Load/Start/Memory/Dump 动作自动保障 Agent 在线；加载后刷新模块；Dump+Fix 使用真实模块列表下拉。
- 已把 Flutter 顶部主入口改为自动会话选择、一键加载 Agent、刷新模块、Dump+Fix 常驻动作，不再把手动 PID 作为主控制路径。
- 已修 Flutter 顶部“自动发现目标进程”命令行左侧对齐：移除额外 24px 左右缩进，使其与标题和状态组件使用同一外层 padding 起点。
- 已修 Flutter 顶部 `VD-Trace` 标题视觉：改为带外圈 radar 图案的品牌块，并新增几何测试保证品牌块与自动目标行左侧对齐。
- 已继续修 Flutter GUI 对齐老 Python GUI 主路径：顶部删除 Loader 会话下拉/手动目标选择入口，改成自动发现状态展示；模块页未拿到真实模块前不再给手填 Dump 模块假入口；Dump+Fix 按钮只依赖自动目标，点击后自动确保 Agent 在线、刷新模块、选择真实模块并执行 dump。
- 已新增控制器级工作流测试：模拟两个 Loader IPC 目标时自动选中 PID 更小的真实目标；Dump+Fix 在 Agent 离线时自动发送 LoadDllRequest、等待上线、刷新模块去重，并对通用测试模块执行 dump。
- 已新增 Flutter Loader IPC 模拟测试：假的 Loader 客户端连接命名管道、发送 AgentHello、接收 LoadDllRequest、回发 LoadDllReply，验证自动会话发现和一键加载协议闭环。
- 已新增并跑通真实 Agent E2E：Flutter LoaderBridge 接真实 Windows 进程内 Loader 客户端，LoadDllRequest 后实际 `LoadLibraryW(VDTraceAgent.dll)` 并调用 `vdtrace_loader_bootstrap`，随后通过 `vdtrace_ctl` 对真实 PID 完成 `modules` 与 `dump VDTraceAgent.dll`，生成 raw/fix dump 文件。
- 已新增并跑通 Flutter 控制器真实 E2E：`VdTraceController` 自动选中真实 Loader IPC 目标，一键加载 Agent 后刷新真实模块列表，并通过控制器 `dumpModule` 完成真实 Dump+Fix。
- 已修 Flutter Loader bridge 常驻稳定性：读循环改为 PeekNamedPipe 非阻塞探测后再读包，避免阻塞读占住同一 pipe handle 影响写请求；stop 时唤醒 pending ConnectNamedPipe 并处理关闭竞态。
- 已重新执行 Flutter analyze/test/build windows --release，并把最新 GUI release 产物覆盖到 `bin/release/`。
- 已修 Flutter Windows runner 无边框交互：去掉 native 顶部 `HTCAPTION` 大块命中，拖拽交回 Flutter 空白标题区；实测最终 exe 样式位已清除原生标题栏/边框并保留 resize/min/max。
- 已删除 `learning/minimal_core_demo/` 教学 demo。
- 已阅读本体 DR/VEH/线程上下文/异常上下文实现，并整理核心流程讲解。
- 已完成 DR 断点、异常上下文处理、VEH 使用方法的概念与实战流程讲解。
- 已按提交范围要求处理：learning/ 保留为本地学习目录，但不纳入 Git 提交范围。

## 3. 经验 / 教训 / 高价值信息

- 仓库根 .gitignore 默认忽略根目录全部内容，只通过白名单放行 release 代码和少量必要配置；非提交目录不要加入白名单。
- 2026-05-09 参考版没有 Flutter GUI，只有 Python/Tk GUI；当前 Flutter GUI 的交互应对齐 Python GUI，不存在“老 Flutter”可参考。
- Flutter SDK 路径：`E:\KDR\flutter\bin\flutter.bat`。
- Loader IPC 协议里 `MessageHeader.size` 是整包大小；`AgentHelloPayload` 是 processPath[1024] 后跟 protocolVersion/featureFlags，不是先读两个 uint32。
- Flutter bridge 不能在常驻空闲轮询里反复创建 isolate；PeekNamedPipe 是非阻塞快路径，只有实际 ReadFile/ConnectNamedPipe 这种阻塞 I/O 才放到后台 isolate。
