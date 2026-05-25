# Task-Status

## 1. 当前任务 / 需求 / 待办清单

### 参考远程 VD-Trace UI 交互
- [x] 拉取 `https://github.com/zushinzackery2-ship-it/VD-Trace` 到临时目录，只做交互参考
- [x] 提炼非布局 UI 交互策略（按钮放置、日志页动作、状态反馈、操作流）
- [x] 将适合项落地到当前 Flutter GUI，禁止跨项目代码引用
- [x] analyze/test/build 并覆盖 `bin/release/`

### Flutter 整体布局检查与重整
- [x] 检查当前顶部栏 / 命令栏 / 分页栏布局层级
- [x] 重整为纯标题拖拽栏 + 命令/状态同排 + 单一分页主内容
- [x] analyze/test/build 并覆盖 `bin/release/`

### Python GUI -> Flutter GUI 替换
- [x] 使用 `E:\KDR\flutter\bin\flutter.bat` 在 `src/flutter_gui/` 建立 Flutter Windows 控制端
- [x] 移植 Python GUI 的控制契约：`vdtrace_ctl.exe` 调用、`TraceConfig` 参数生成、`vdtrace_gui.ini` 读写、状态格式化、trace preview、memory 编码、depth filter、observer/probe 规则
- [x] 实现 Flutter 页面：会话/Agent 加载、追踪策略、深度过滤、观测器、内存 R/W、trace preview、Loader 日志
- [x] 处理 Loader 命名管道：Flutter bridge 已接收会话 hello/log；Agent 加载请求未启用直接注入 fallback，后续需要长连接 pipe 完整化
- [x] README 源码结构与构建说明切到 Flutter GUI
- [x] 跑 Flutter analyze/test/build，并保留 Python 自检作为迁移对照直到功能等价

### Review 修复（5个确认问题）
- [ ] `src/agent/VDTraceAgentIpc.h/.cpp`：`thread_` 句柄泄漏，析构为 default，无 WaitForSingleObject + CloseHandle
- [ ] `src/bepinex_plugin/VDTraceAutoStartPlugin.csproj`：HintPath 硬编码 `F:\Program Files\...`
- [ ] `src/python_gui/vdtrace_gui/console_support.py` + `vdtrace_gui.py`：测试数据重复
- [ ] `src/VDTraceRuntime.cpp:161`：magic number `2000`（超时阈值）
- [ ] `src/VDTraceAsyncHandoff.cpp:60`：magic number `200`（重试阈值）

### 交付流程
- [ ] 轻量测试通过
- [ ] 冗余逻辑/僵尸代码清理
- [ ] 综合优化
- [x] VehTrace 仓库新建 new 分支推送远程
- [ ] WinHttp 推送到 WinHttpRedirectProxy 仓库 main 分支


## 2. 已解决问题 / 已完成需求

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
  - VDTRACE 侧：删 `src/loader/`（15个文件）+ 删 `VDTraceLoaderAdapter.h`；`VDTraceLoaderControlSupport.cpp` 命名空间统一；Python GUI `models.py` magic/pipe 名统一到 ref_pic；`build_shared.bat` 加 `ENABLE_PROTOCOL_VERSION` 宏
  - 回归通过：`build_shared.bat full` 零 error、session smoke 9 cases passed、smoke suite 全 passed
- 已新增 `flutter-windows-frameless` skill 到 opencode 与 Codex，沉淀 Flutter Windows runner 无系统标题栏、Flutter 自绘标题栏、Win32 命中测试和最终 exe style 验证流程
- VehTrace 仓库 winhttp-unification 分支已推送到远程：`git push origin winhttp-unification` 成功

## 3. 经验 / 教训 / 高价值信息

- Flutter SDK 路径：`E:\KDR\flutter\bin\flutter.bat`，当前可用版本 Flutter 3.38.1 / Dart 3.10.0
- Flutter Loader bridge 当前可接收 `AgentHello` / `AgentLog`，但还未持有长连接 pipe 执行 `LoadDllRequest`；界面明确阻止回退直接注入，避免恢复旧 fallback 语义
- `src/python_gui/` 不只是 tkinter UI，还包含 CLI、配置迁移、协议/参数编码和自检资产；替换时不能整包先删
- Flutter GUI 应继续以 `vdtrace_ctl.exe` 作为 Agent IPC seam，Loader pipe 另行实现 bridge，避免 Dart UI 直接碰 Agent IPC 结构体
- ref_pic 通过编译宏（`ENABLE_PROTOCOL_VERSION` / `ENABLE_BOOTSTRAP` / `WHITELIST_PROCESS`）实现零侵入增强
- 工具侧编译段也需要 `ENABLE_PROTOCOL_VERSION` 宏，否则 `AgentHelloPayload` 大小不匹配
- Review 中 6 个 magic number 报告有 5 个是误判（已有 constexpr/const 定义），只有 2 个是真问题
