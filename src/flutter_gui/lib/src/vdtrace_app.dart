import 'dart:async';

import 'package:flutter/material.dart';

import 'ui_theme.dart';
import 'ui_widgets.dart';
import 'vdtrace_controller.dart';
import 'window_control.dart';

class VdTraceFlutterApp extends StatelessWidget
{
  const VdTraceFlutterApp({super.key, this.startRuntime = true});

  final bool startRuntime;

  @override
  Widget build(BuildContext context)
  {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'VD-Trace',
      theme: buildVdTraceTheme(),
      home: VdTraceHomePage(startRuntime: startRuntime),
    );
  }
}

class VdTraceHomePage extends StatefulWidget
{
  const VdTraceHomePage({super.key, required this.startRuntime});

  final bool startRuntime;

  @override
  State<VdTraceHomePage> createState() => _VdTraceHomePageState();
}

class _VdTraceHomePageState extends State<VdTraceHomePage>
{
  late final VdTraceController controller;
  late final Timer uiTimer;
  final moduleController = TextEditingController();
  final memoryAddressController = TextEditingController();
  final memorySizeController = TextEditingController(text: '64');
  final memoryValueController = TextEditingController();
  String memoryMode = 'HEX';

  @override
  void initState()
  {
    super.initState();
    controller = VdTraceController();
    if (widget.startRuntime)
    {
      controller.start();
    }
    moduleController.text = controller.profile.modules;
    uiTimer = Timer.periodic(const Duration(milliseconds: 500), (_)
    {
      controller.syncSessions();
      final selectedModule = controller.selectedDumpModule;
      if (selectedModule != null && moduleController.text != selectedModule)
      {
        moduleController.text = selectedModule;
      }
      setState(() {});
    });
  }

  @override
  void dispose()
  {
    uiTimer.cancel();
    if (widget.startRuntime)
    {
      controller.dispose();
    }
    moduleController.dispose();
    memoryAddressController.dispose();
    memorySizeController.dispose();
    memoryValueController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context)
  {
    return Scaffold(
      body: Container(
        decoration: const BoxDecoration(
          gradient: LinearGradient(begin: Alignment.topLeft, end: Alignment.bottomRight, colors: [Color(0xffffffff), vdBg]),
        ),
        child: SafeArea(
          child: Column(
            children: [
              _TopCommandBar(controller: controller, moduleController: moduleController, onModuleChanged: _selectDumpModule, onRun: _run),
              Expanded(child: _mainTabs()),
            ],
          ),
        ),
      ),
    );
  }

  Widget _mainTabs()
  {
    final profile = controller.profile;
    final loaderLog = controller.loaderLogs.map((entry) => entry.text).join('\n');
    return DefaultTabController(
      length: 8,
      child: Column(
        children: [
          const _ConfigTabs(),
          Expanded(
            child: TabBarView(
              children: [
                PagePad(children: [SectionCard(title: 'Core Trace Setup', subtitle: '启动前最常改的参数集中在这里。', child: Column(children: [FieldGrid(
            children: [
              TextFieldRow(label: 'Agent DLL', value: profile.agentPath, onChanged: (value) => profile.agentPath = value),
              TextFieldRow(label: '追踪模块', value: profile.modules, onChanged: (value) => profile.modules = value),
              TextFieldRow(label: '输出路径', value: profile.outputPath, onChanged: (value) => profile.outputPath = value),
              TextFieldRow(label: '触发点', value: profile.triggerPoint, onChanged: (value) => profile.triggerPoint = value),
              TextFieldRow(label: '线程 ID', value: profile.threadId, onChanged: (value) => profile.threadId = value),
              TextFieldRow(label: '事件上限', value: profile.maxEvents, onChanged: (value) => profile.maxEvents = value),
              DropdownRow(label: '后端', value: profile.backend, values: const ['DR', 'TF', 'PT'], onChanged: (value) => setState(() => profile.backend = value)),
            ],
          ), Align(alignment: Alignment.centerLeft, child: Wrap(spacing: 12, runSpacing: 8, children: [ActionButton(label: '一键加载 Agent', icon: Icons.login, onPressed: controller.canLoadAgent ? () => _run(controller.loadAgent) : null), ActionButton(label: '刷新模块', icon: Icons.refresh, onPressed: controller.canRefreshModules ? () => _run(controller.refreshModules) : null)]))]))]),
                PagePad(children: [SectionCard(title: 'Run Policy', subtitle: '运行行为开关，直接影响 configure 参数。', child: FieldGrid(
            children: [
              SwitchRow(label: '自动线程捕获', value: profile.threadCapture, onChanged: (value) => setState(() => profile.threadCapture = value)),
              SwitchRow(label: '定点触发', value: profile.triggerEnabled, onChanged: (value) => setState(() => profile.triggerEnabled = value)),
              SwitchRow(label: '阻塞主线程', value: profile.blockMainThread, onChanged: (value) => setState(() => profile.blockMainThread = value)),
              SwitchRow(label: '记录模块外', value: profile.traceOutsideModules, onChanged: (value) => setState(() => profile.traceOutsideModules = value)),
              SwitchRow(label: '重复命中', value: profile.repeatHits, onChanged: (value) => setState(() => profile.repeatHits = value)),
              SwitchRow(label: '增强采样', value: profile.enhancedSampling, onChanged: (value) => setState(() => profile.enhancedSampling = value)),
              SwitchRow(label: '根返回停止', value: profile.rootStopOnReturn, onChanged: (value) => setState(() => profile.rootStopOnReturn = value)),
              SwitchRow(label: '异步线程接力', value: profile.asyncThreadHandoff, onChanged: (value) => setState(() => profile.asyncThreadHandoff = value)),
            ],
          ))]),
                PagePad(children: [SectionCard(title: 'Depth Filter', subtitle: controller.depthSummary(), child: FieldGrid(
            children: [
              TextFieldRow(label: '默认层级', value: profile.callDepth, onChanged: (value) => profile.callDepth = value),
              TextFieldRow(label: '模块规则', value: profile.moduleCallDepths, onChanged: (value) => profile.moduleCallDepths = value),
              SwitchRow(label: '模块外规则', value: profile.outsideCallDepthEnabled, onChanged: (value) => setState(() => profile.outsideCallDepthEnabled = value)),
              TextFieldRow(label: '模块外层级', value: profile.outsideCallDepth, onChanged: (value) => profile.outsideCallDepth = value),
              DropdownRow(label: '模块外模式', value: profile.outsideExecutionMode, values: const ['EDGE', 'TF'], onChanged: (value) => setState(() => profile.outsideExecutionMode = value)),
              SwitchRow(label: '匿名页规则', value: profile.anonymousExecCallDepthEnabled, onChanged: (value) => setState(() => profile.anonymousExecCallDepthEnabled = value)),
              TextFieldRow(label: '匿名页层级', value: profile.anonymousExecCallDepth, onChanged: (value) => profile.anonymousExecCallDepth = value),
              DropdownRow(label: '匿名页模式', value: profile.anonymousExecExecutionMode, values: const ['EDGE', 'TF'], onChanged: (value) => setState(() => profile.anonymousExecExecutionMode = value)),
              SwitchRow(label: '空转跳出', value: profile.idleEscapeEnabled, onChanged: (value) => setState(() => profile.idleEscapeEnabled = value)),
              TextFieldRow(label: '空转阈值', value: profile.idleEscapeThreshold, onChanged: (value) => profile.idleEscapeThreshold = value),
            ],
          ))]),
                PagePad(children: [SectionCard(title: 'Observer / Probe', subtitle: 'Capture / Step / Write probe 规则。', child: Column(
            children: [
              SwitchRow(label: '启用观测器', value: profile.probeEnabled, onChanged: (value) => setState(() => profile.probeEnabled = value)),
              TextFieldRow(label: '规则', value: profile.probeSpec, maxLines: 7, onChanged: (value) => profile.probeSpec = value),
            ],
          ))]),
                PagePad(children: [SectionCard(title: 'Memory Tool', subtitle: '调试时临时读写内存。', child: Column(
            children: [
              FieldGrid(
                children: [
                  TextFieldRow(label: '地址', value: '', controller: memoryAddressController, onChanged: (_) {}),
                  TextFieldRow(label: '读取长度', value: '64', controller: memorySizeController, onChanged: (_) {}),
                  DropdownRow(label: '写入模式', value: memoryMode, values: const ['HEX', 'TEXT', 'UTF16', 'U32', 'U64'], onChanged: (value) => setState(() => memoryMode = value)),
                  TextFieldRow(label: '写入值', value: '', controller: memoryValueController, onChanged: (_) {}),
                ],
              ),
              Align(
                alignment: Alignment.centerLeft,
                child: Wrap(spacing: 12, children: [
                  ActionButton(label: '读取', icon: Icons.download, onPressed: controller.canReadMemory ? () => _run(() => controller.readMemory(memoryAddressController.text, memorySizeController.text)) : null),
                  ActionButton(label: '写入', icon: Icons.upload, onPressed: controller.canWriteMemory ? () => _run(() => controller.writeMemory(memoryAddressController.text, memoryMode, memoryValueController.text)) : null),
                ]),
              ),
              TextPanel(title: '最近内存结果', text: controller.memoryResultText, minHeight: 120),
            ],
          ))]),
                PagePad(children: [SectionCard(title: 'Trace Control', subtitle: controller.statusText, child: Wrap(spacing: 12, runSpacing: 8, children: [ActionButton(label: '开始追踪', icon: Icons.play_arrow, onPressed: controller.canStartTrace ? () => _run(controller.startTrace) : null), ActionButton(label: '停止追踪', icon: Icons.stop, onPressed: controller.canStopTrace ? () => _run(controller.stopTrace) : null)])), TextPanel(title: controller.previewStatus, text: controller.previewText, minHeight: 430)]),
                PagePad(children: [SectionCard(title: 'Log Control', subtitle: '看到异常输出时可就地停止追踪。', child: Align(alignment: Alignment.centerLeft, child: ActionButton(label: '停止追踪', icon: Icons.stop, onPressed: controller.canStopTrace ? () => _run(controller.stopTrace) : null))), TextPanel(title: '动作日志', text: controller.outputLog, minHeight: 420), TextPanel(title: 'Loader 日志', text: loaderLog, minHeight: 220)]),
                PagePad(children: [SectionCard(title: 'Module Tools', subtitle: controller.moduleNames.isEmpty ? '等待 Agent 上线后自动刷新真实模块列表。' : '已发现 ${controller.moduleNames.length} 个真实模块。', child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [ModulePicker(key: const ValueKey('page_module_picker'), controller: controller, controllerText: moduleController, onChanged: _selectDumpModule), const SizedBox(height: 10), Align(alignment: Alignment.centerLeft, child: Wrap(spacing: 12, runSpacing: 8, children: [ActionButton(label: '刷新模块', icon: Icons.refresh, onPressed: controller.canRefreshModules ? () => _run(controller.refreshModules) : null), ActionButton(label: 'Dump+Fix', icon: Icons.inventory_2, onPressed: controller.canDumpModule ? () => _run(() => controller.dumpModule(controller.selectedDumpModule ?? '')) : null)]))])), TextPanel(title: '模块列表', text: controller.moduleList, minHeight: 420)]),
              ],
            ),
          ),
        ],
      ),
    );
  }

  Future<void> _run(Future<void> Function() action) async
  {
    await action();
    setState(() {});
  }

  void _selectDumpModule(String value)
  {
    setState(()
    {
      controller.selectedDumpModule = value;
      moduleController.text = value;
    });
  }
}

class _TopCommandBar extends StatelessWidget
{
  const _TopCommandBar({required this.controller, required this.moduleController, required this.onModuleChanged, required this.onRun});

  final VdTraceController controller;
  final TextEditingController moduleController;
  final void Function(String value) onModuleChanged;
  final Future<void> Function(Future<void> Function() action) onRun;

  @override
  Widget build(BuildContext context)
  {
    return Container(
      padding: const EdgeInsets.fromLTRB(14, 10, 12, 10),
      decoration: const BoxDecoration(color: vdPanel, border: Border(bottom: BorderSide(color: vdLine))),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const _BrandTitle(),
              const SizedBox(width: 16),
              Expanded(
                child: GestureDetector(
                  behavior: HitTestBehavior.opaque,
                  onPanDown: (_) => startWindowDrag(),
                  onDoubleTap: maximizeOrRestoreWindow,
                  child: const SizedBox(height: 32),
                ),
              ),
              const _WindowButtons(),
            ],
          ),
          const SizedBox(height: 10),
          Row(
            children: [
              Expanded(child: _CommandWrap(controller: controller, moduleController: moduleController, onModuleChanged: onModuleChanged, onRun: onRun)),
            ],
          ),
          const SizedBox(height: 8),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              StatPill(label: 'PID', value: '${controller.selectedPid ?? '-'}', color: vdBlue),
              StatPill(label: 'TRACE', value: controller.traceRunning ? 'RUNNING' : 'IDLE', color: controller.traceRunning ? vdCyan : vdTextMuted),
              StatPill(label: 'WRITE', value: controller.traceWriting ? 'BUSY' : 'READY', color: controller.traceWriting ? vdAmber : vdTextMuted),
              StatPill(label: 'BACKEND', value: controller.profile.backend, color: vdBlue),
              StatPill(label: 'SESSIONS', value: '${controller.sessions.length}', color: vdCyan),
            ],
          ),
          const SizedBox(height: 6),
          Text(controller.statusText, maxLines: 1, overflow: TextOverflow.ellipsis, style: const TextStyle(color: vdTextStrong, fontWeight: FontWeight.w600, fontSize: 12)),
        ],
      ),
    );
  }
}

class _BrandTitle extends StatelessWidget
{
  const _BrandTitle();

  @override
  Widget build(BuildContext context)
  {
    return Container(
      key: const ValueKey('vdtrace_brand_title'),
      height: 38,
      padding: const EdgeInsets.fromLTRB(4, 3, 12, 3),
      decoration: BoxDecoration(
        color: vdPanelSoft,
        border: Border.all(color: vdLine),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: 30,
            height: 30,
            decoration: BoxDecoration(
              color: vdBlue,
              shape: BoxShape.circle,
              border: Border.all(color: vdCyan.withValues(alpha: 0.55)),
              boxShadow: [BoxShadow(color: vdBlue.withValues(alpha: 0.18), blurRadius: 10, offset: const Offset(0, 2))],
            ),
            child: const Icon(Icons.radar, size: 18, color: Colors.white),
          ),
          const SizedBox(width: 9),
          const Text('VD-Trace', style: TextStyle(fontSize: 21, fontWeight: FontWeight.w900, letterSpacing: 0)),
        ],
      ),
    );
  }
}

class _CommandWrap extends StatelessWidget
{
  const _CommandWrap({required this.controller, required this.moduleController, required this.onModuleChanged, required this.onRun});

  final VdTraceController controller;
  final TextEditingController moduleController;
  final void Function(String value) onModuleChanged;
  final Future<void> Function(Future<void> Function() action) onRun;

  @override
  Widget build(BuildContext context)
  {
    return Wrap(
      spacing: 8,
      runSpacing: 8,
      crossAxisAlignment: WrapCrossAlignment.center,
      children: [
        SizedBox(width: 430, child: _AutoTargetPanel(controller: controller)),
        SizedBox(
          width: 260,
          child: ModulePicker(
            key: const ValueKey('top_module_picker'),
            controller: controller,
            controllerText: moduleController,
            onChanged: onModuleChanged,
          ),
        ),
        ActionButton(label: '一键加载 Agent', icon: Icons.login, onPressed: controller.canLoadAgent ? () => onRun(controller.loadAgent) : null),
        ActionButton(label: '刷新模块', icon: Icons.refresh, onPressed: controller.canRefreshModules ? () => onRun(controller.refreshModules) : null),
        ActionButton(label: 'Dump+Fix', icon: Icons.inventory_2, onPressed: controller.canDumpModule ? () => onRun(() => controller.dumpModule(controller.selectedDumpModule ?? '')) : null),
      ],
    );
  }
}

class _AutoTargetPanel extends StatelessWidget
{
  const _AutoTargetPanel({required this.controller});

  final VdTraceController controller;

  @override
  Widget build(BuildContext context)
  {
    final session = controller.selectedSession;
    return Container(
      height: 58,
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
      decoration: BoxDecoration(color: vdPanelSoft, borderRadius: BorderRadius.circular(8), border: Border.all(color: vdLine)),
      child: Row(
        children: [
          Icon(session == null ? Icons.radar : Icons.memory, size: 20, color: session == null ? vdTextMuted : vdCyan),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisAlignment: MainAxisAlignment.center,
              children: [
                Text(controller.autoTargetTitle, maxLines: 1, overflow: TextOverflow.ellipsis, style: const TextStyle(fontWeight: FontWeight.w800, color: vdTextStrong, fontSize: 13)),
                const SizedBox(height: 2),
                Text(controller.autoTargetSubtitle, maxLines: 1, overflow: TextOverflow.ellipsis, style: const TextStyle(color: vdTextMuted, fontSize: 11)),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class ModulePicker extends StatelessWidget
{
  const ModulePicker({super.key, required this.controller, required this.controllerText, required this.onChanged});

  final VdTraceController controller;
  final TextEditingController controllerText;
  final void Function(String value) onChanged;

  @override
  Widget build(BuildContext context)
  {
    final modules = controller.moduleNames;
    if (modules.isEmpty)
    {
      return InputDecorator(
        decoration: const InputDecoration(labelText: 'Dump 模块', prefixIcon: Icon(Icons.inventory_2)),
        child: Text(
          controller.hasTarget ? '真实模块列表将自动刷新' : '等待自动发现目标',
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: const TextStyle(color: vdTextMuted),
        ),
      );
    }
    final selected = modules.contains(controller.selectedDumpModule) ? controller.selectedDumpModule! : modules.first;
    return DropdownButtonFormField<String>(
      initialValue: selected,
      decoration: const InputDecoration(labelText: 'Dump 模块', prefixIcon: Icon(Icons.inventory_2)),
      items: [for (final module in modules) DropdownMenuItem(value: module, child: Text(module, overflow: TextOverflow.ellipsis))],
      onChanged: (value)
      {
        if (value == null)
        {
          return;
        }
        controllerText.text = value;
        onChanged(value);
      },
    );
  }
}

class _WindowButtons extends StatelessWidget
{
  const _WindowButtons();

  @override
  Widget build(BuildContext context)
  {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        _WindowButton(icon: Icons.remove, tooltip: '最小化', onPressed: minimizeWindow),
        _WindowButton(icon: Icons.crop_square, tooltip: '最大化/还原', onPressed: maximizeOrRestoreWindow),
        _WindowButton(icon: Icons.close, tooltip: '关闭', danger: true, onPressed: closeWindow),
      ],
    );
  }
}

class _WindowButton extends StatelessWidget
{
  const _WindowButton({required this.icon, required this.tooltip, required this.onPressed, this.danger = false});

  final IconData icon;
  final String tooltip;
  final VoidCallback onPressed;
  final bool danger;

  @override
  Widget build(BuildContext context)
  {
    return Tooltip(
      message: tooltip,
      child: InkWell(
        borderRadius: BorderRadius.circular(8),
        onTap: onPressed,
        child: Container(
          width: 34,
          height: 30,
          margin: const EdgeInsets.only(left: 4),
          decoration: BoxDecoration(color: danger ? const Color(0xffffeeee) : vdPanelSoft, borderRadius: BorderRadius.circular(8), border: Border.all(color: vdLine)),
          child: Icon(icon, size: 15, color: danger ? const Color(0xffdc2626) : vdTextStrong),
        ),
      ),
    );
  }
}

class _ConfigTabs extends StatelessWidget
{
  const _ConfigTabs();

  @override
  Widget build(BuildContext context)
  {
    return Container(
      margin: const EdgeInsets.fromLTRB(14, 10, 14, 0),
      child: Center(
        child: Container(
          padding: const EdgeInsets.all(5),
          decoration: BoxDecoration(color: vdPanel, border: Border.all(color: vdLine), borderRadius: BorderRadius.circular(14)),
          child: const TabBar(
            isScrollable: true,
            tabAlignment: TabAlignment.center,
            tabs: [
              Tab(icon: Icon(Icons.tune, size: 16), text: '核心'),
              Tab(icon: Icon(Icons.toggle_on, size: 16), text: '策略'),
              Tab(icon: Icon(Icons.account_tree, size: 16), text: '过滤'),
              Tab(icon: Icon(Icons.radar, size: 16), text: '观测'),
              Tab(icon: Icon(Icons.storage, size: 16), text: '内存'),
              Tab(icon: Icon(Icons.subject, size: 16), text: '预览'),
              Tab(icon: Icon(Icons.terminal, size: 16), text: '日志'),
              Tab(icon: Icon(Icons.view_list, size: 16), text: '模块'),
            ],
          ),
        ),
      ),
    );
  }
}
