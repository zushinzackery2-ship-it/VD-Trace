# Task-Status

## 1. 当前任务 / 需求 / 待办清单

- 当前无未完成交付项


## 2. 已解决问题 / 已完成需求

- README 已同步当前控制入口：新增 Flutter GUI / C++ CLI / autostart / BepInEx / early_loader 入口边界，明确 Python/Tkinter legacy GUI/CLI 已下线删除，并修正早期加载器构建段落格式
- Python legacy GUI/CLI 控制端已下线并删除：当前控制入口收敛为 Flutter Windows GUI、`vdtrace_ctl.exe`、`vdtrace_autostart.exe`；旧 `src/python_gui/`、`vdtrace_cli.py`、`vdtrace_gui.py` 引用复查为 0；C++ Release 构建通过；Release smoke suite 通过；smoke 派生 static_refs/bin/dump 产物已清理
- Review 5 项已收敛：Agent IPC 停止链路可唤醒阻塞中的管道连接等待，并通过 `CancelSynchronousIo` 覆盖已连接后的同步 I/O 阻塞；BepInEx 插件已使用 NuGet 包引用；runtime/async 两处阈值已使用命名常量；C++ Release 构建和 smoke suite 通过，临时 static_refs/dump/bin 已清理
- Python GUI -> Flutter GUI 替换已完成：Flutter Windows 控制端已建立，核心控制契约、配置、trace preview、memory、depth filter、observer/probe、Loader 日志页面已落地；Flutter analyze/test/build 通过，产物已覆盖 `bin/release/`
- Flutter 整体布局重整已完成：顶部拆为纯标题拖拽栏 + 命令/状态同排 + 单一分页主内容，拖拽、双击最大化、无系统标题栏和亮色主题均已收敛
- 远程 VD-Trace UI 交互参考已落地：按钮归位、日志页动作、模块页刷新/Dump、内存最近结果、控制器禁用态和状态反馈已完成
- VehTrace 仓库新建 new 分支推送远程已完成

- 模块目录重整已完成：`src/VDTrace*.cpp/.h` 平铺核心源码已迁入 `src/core/` 职责目录；控制端共享层已拆到 `src/control/`；CLI 入口已拆到 `src/tools/vdtrace_ctl/`、`src/tools/vdtrace_autostart/`、`src/tools/examples/`；BepInEx 插件迁入 `src/plugins/bepinex/` 并改为 NuGet 包引用；早期加载器迁入 `src/loaders/early_loader/`；README 已同步；旧路径引用和旧 `src/bepinex_plugin/obj` 生成产物已清理；`.clangd` 已补充 include 根，关键文件 LSP 检查 clean；`dotnet build src\plugins\bepinex\VDTraceAutoStartPlugin.csproj -c Release` 通过（NU1603 版本替代 warning）；C++ Release x64 构建入口已切到 `build_release.bat` + CMake/VS2022，输出 `bin/release/`，中间文件 `obj/cmake-x64-release/`
- C++ Release x64 构建与 smoke 收尾已完成：`cmd.exe /c "call build_release.bat"` 构建通过；`cmd.exe /c "cd /d E:\科研\VD-Trace\bin\release && vdtrace_smoke_suite_test.exe"` 全量 smoke 通过；解密 smoke 已显式使用 TF/full 追踪覆盖 helper DLL 与匿名执行页，decoder ret 事件已携带返回目标以稳定标记离开动态执行页；测试日志、static_refs 和 dump 临时产物已清理
- 更新README.md，添加新模块（BepInEx插件、早期加载器、HeapPeek、Extender）和功能说明，更新源码结构和构建说明，已推送到远程仓库
- Flutter 主分页栏已居中：TabBar 外层 Center，`tabAlignment: TabAlignment.center`；analyze/test/build 通过并覆盖 `bin/release/`
- 参考远程 VD-Trace UI 后完成交互归位：顶部只保留 PID/会话/状态；加载 Agent 放核心页；开始/停止放预览页，日志页保留停止；刷新模块/Dump 放模块页；内存页增加最近结果；按钮统一使用控制器 can* getter 禁用；analyze/test/build 通过并覆盖 `bin/release/`
- Flutter 单主内容分页布局已完成：删除右边监控栏，预览/日志/模块列表并入主分页；已 kill 占用的 `vdtrace_gui.exe` 并覆盖 `bin/release/`
- Flutter 标题栏结构已修正：顶部拆成独立标题拖拽栏 + 命令控件栏；拖动改为 root HWND 发送 `WM_SYSCOMMAND/SC_MOVE|HTCAPTION`；清理重复旧类和注释遗留；analyze/test/build 通过并覆盖 `bin/release/`
- Flutter 拖动与分页已修复：标题右侧空白区由 Flutter 主动发 `WM_NCLBUTTONDOWN/HTCAPTION` 触发拖动，双击最大化/还原；配置区恢复分页：核心/策略/过滤/观测/内存；analyze/test/build 通过并覆盖 `bin/release/`
- Flutter 顶部拖拽区已修复：`WM_NCHITTEST` 在 Flutter runner 处理前交给 Win32Window，标题右侧增加空白拖拽区域，控件不再全顶到左侧；analyze/test/build 通过并覆盖 `bin/release/`
- Flutter GUI 密度和窗口 chrome 已调整：整体 padding/字号/控件高度缩小；Windows runner 改无系统标题栏的 resizable popup；Flutter 顶部右上角新增最小化/最大化/关闭三件套；左上标题区支持拖动；analyze/test/build 通过并覆盖 `bin/release/`
- Flutter GUI 已切换亮色主题：浅色背景、白色卡片、浅色输入框、浅色终端面板、蓝/青/琥珀状态强调；analyze/test/build 通过并覆盖 `bin/release/`
- Flutter GUI 二次布局重构：改为顶部目标/状态/动作常驻，下方配置/监控双栏；移除左侧目标栏和右侧硬固定三栏结构；顶部命令区响应式换行；analyze/test/build 通过并覆盖 `bin/release/`
- Flutter GUI 布局重设计：取消主区 Tab 切页，改为左侧目标、中间工作台、右侧实时监控三栏；高频动作和核心参数常驻，运行策略/深度过滤/观测器/内存工具用折叠区块；右侧预览/日志/模块列表可滚动常驻；analyze/test/build 通过并覆盖 `bin/release/`
- 修复 Flutter GUI 启动未响应：`LoaderBridge` 原先在 UI isolate 同步 `ConnectNamedPipe`，无 Loader 连接时阻塞窗口消息循环；已改 `PIPE_NOWAIT` 非阻塞轮询并处理 `ERROR_PIPE_LISTENING`，analyze/test/build 通过并覆盖 `bin/release/`
- Flutter GUI 视觉重做：暗色仪表盘布局、状态 pill、侧边会话卡片、分组卡片、终端输出面板；拆出 `ui_theme.dart` / `ui_widgets.dart` 降低 UI 文件复杂度；analyze/test/build 通过并已重新集成到 `bin/release/`
- Flutter release 产物已集成到 `bin/release/`：`vdtrace_gui.exe`、`flutter_windows.dll`、`native_assets.json`、`data/`
- 已清理 `bin/release/` 构建产物、测试 exe、log/static_refs、dump/traces 输出；`bin/` 当前为空目录
- Flutter GUI 初版已落地：新增 `src/flutter_gui/`，`flutter analyze` / `flutter test` / `flutter build windows --release` 均通过，产物为 `src/flutter_gui/build/windows/x64/runner/Release/vdtrace_gui.exe`
- WinHttp 代理统一迁移已完成：
  - ref_pic 增强：`ipc_control.hpp` 加 `kProtocolVersion`/`kFeatureBootstrapAfterLoad` + `AgentHelloPayload` 条件字段 + `SendAgentHello` 填充；`redirect_runtime.cpp` 加 `#ifdef ENABLE_BOOTSTRAP` 和 `#ifdef WHITELIST_PROCESS` 分支
  - VDTRACE 侧：删 `src/loader/`（15个文件）+ 删 `VDTraceLoaderAdapter.h`；`VDTraceLoaderControlSupport.cpp` 命名空间统一；`build_shared.bat` 加 `ENABLE_PROTOCOL_VERSION` 宏
  - 回归通过：`build_shared.bat full` 零 error、session smoke 9 cases passed、smoke suite 全 passed
- 已新增 `flutter-windows-frameless` skill 到 opencode 与 Codex，沉淀 Flutter Windows runner 无系统标题栏、Flutter 自绘标题栏、Win32 命中测试和最终 exe style 验证流程
- VehTrace 仓库 winhttp-unification 分支已推送到远程：`git push origin winhttp-unification` 成功

## 3. 经验 / 教训 / 高价值信息

- Flutter SDK 路径：`E:\KDR\flutter\bin\flutter.bat`，当前可用版本 Flutter 3.38.1 / Dart 3.10.0
- Flutter Loader bridge 当前可接收 `AgentHello` / `AgentLog`，但还未持有长连接 pipe 执行 `LoadDllRequest`；界面明确阻止回退直接注入，避免恢复旧 fallback 语义
- Python legacy GUI/CLI 已删除；Flutter GUI 和 C++ CLI 是当前控制入口，后续协议和配置演进不要再回填旧 Tkinter/Python 链路
- Flutter GUI 应继续以 `vdtrace_ctl.exe` 作为 Agent IPC seam，Loader pipe 另行实现 bridge，避免 Dart UI 直接碰 Agent IPC 结构体
- ref_pic 通过编译宏（`ENABLE_PROTOCOL_VERSION` / `ENABLE_BOOTSTRAP` / `WHITELIST_PROCESS`）实现零侵入增强
- 工具侧编译段也需要 `ENABLE_PROTOCOL_VERSION` 宏，否则 `AgentHelloPayload` 大小不匹配
- Review 中 6 个 magic number 报告有 5 个是误判（已有 constexpr/const 定义），只有 2 个是真问题
