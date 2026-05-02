# Task-Status

## 1. 当前任务 / 需求 / 待办清单

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
- [ ] VehTrace 仓库新建 new 分支推送远程
- [ ] WinHttp 推送到 WinHttpRedirectProxy 仓库 main 分支

## 2. 已解决问题 / 已完成需求

- WinHttp 代理统一迁移已完成：
  - ref_pic 增强：`ipc_control.hpp` 加 `kProtocolVersion`/`kFeatureBootstrapAfterLoad` + `AgentHelloPayload` 条件字段 + `SendAgentHello` 填充；`redirect_runtime.cpp` 加 `#ifdef ENABLE_BOOTSTRAP` 和 `#ifdef WHITELIST_PROCESS` 分支
  - VDTRACE 侧：删 `src/loader/`（15个文件）+ 删 `VDTraceLoaderAdapter.h`；`VDTraceLoaderControlSupport.cpp` 命名空间统一；Python GUI `models.py` magic/pipe 名统一到 ref_pic；`build_shared.bat` 加 `ENABLE_PROTOCOL_VERSION` 宏
  - 回归通过：`build_shared.bat full` 零 error、session smoke 9 cases passed、smoke suite 全 passed

## 3. 经验 / 教训 / 高价值信息

- ref_pic 通过编译宏（`ENABLE_PROTOCOL_VERSION` / `ENABLE_BOOTSTRAP` / `WHITELIST_PROCESS`）实现零侵入增强
- 工具侧编译段也需要 `ENABLE_PROTOCOL_VERSION` 宏，否则 `AgentHelloPayload` 大小不匹配
- Review 中 6 个 magic number 报告有 5 个是误判（已有 constexpr/const 定义），只有 2 个是真问题
