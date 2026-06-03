import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'models.dart';
import 'ui_theme.dart';
import 'ui_widgets.dart';
import 'vdtrace_controller.dart';
import 'vdtrace_top_bar.dart';

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

class _VdTraceHomePageState extends State<VdTraceHomePage> with TickerProviderStateMixin
{
  late final VdTraceController controller;
  late final TabController tabController;
  final moduleController = TextEditingController();
  final memoryAddressController = TextEditingController();
  final memorySizeController = TextEditingController(text: '64');
  final memoryValueController = TextEditingController();
  String memoryMode = 'HEX';

  @override
  void initState()
  {
    super.initState();
    tabController = TabController(length: 8, vsync: this);
    controller = VdTraceController();
    if (widget.startRuntime)
    {
      controller.start();
    }
    moduleController.text = controller.profile.modules;
    controller.addListener(_onControllerChanged);
  }

  void _onControllerChanged()
  {
    if (!mounted)
    {
      return;
    }
    final selectedModule = controller.selectedDumpModule;
    if (selectedModule != null && moduleController.text != selectedModule)
    {
      moduleController.text = selectedModule;
    }
    final error = controller.lastError;
    if (error != null)
    {
      controller.lastError = null;
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(
        content: Text(error),
        backgroundColor: const Color(0xffdc2626),
        behavior: SnackBarBehavior.floating,
        duration: const Duration(seconds: 4),
      ));
    }
    setState(() {});
  }

  @override
  void dispose()
  {
    controller.removeListener(_onControllerChanged);
    if (widget.startRuntime)
    {
      controller.dispose();
    }
    tabController.dispose();
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
              TopCommandBar(
                controller: controller,
                moduleController: moduleController,
                onModuleChanged: _selectDumpModule,
                onRun: _run,
                onSessionSelected: (LoaderSessionSnapshot session) => setState(() => controller.selectSession(session)),
              ),
              ConfigTabs(controller: tabController),
              Expanded(child: TabBarView(controller: tabController, children: _buildTabPages())),
            ],
          ),
        ),
      ),
    );
  }

  List<Widget> _buildTabPages()
  {
    final p = controller.profile;
    final isBusy = controller.busy;
    final triggerOn = p.triggerEnabled;
    final captureOn = p.threadCapture;

    return [
      _buildCorePage(p, isBusy, triggerOn, captureOn),
      _buildPolicyPage(p, isBusy, triggerOn, captureOn),
      _buildDepthPage(p, isBusy),
      _buildObserverPage(p, isBusy),
      _buildMemoryPage(isBusy),
      _buildTracePage(),
      _buildLogPage(),
      _buildModulePage(),
    ];
  }

  Widget _buildCorePage(TraceProfile p, bool isBusy, bool triggerOn, bool captureOn)
  {
    final threadEnabled = !isBusy && !(triggerOn && captureOn);
    final triggerFieldEnabled = !isBusy && triggerOn;
    return PagePad(children: [SectionCard(title: 'Core Trace Setup', subtitle: '启动前最常改的参数集中在这里。', child: Column(children: [
      FieldGrid(children: [
        TextFieldRow(label: 'Agent DLL', value: p.agentPath, enabled: !isBusy, onChanged: (v) { p.agentPath = v; controller.scheduleProfileSave(); }),
        TextFieldRow(label: '追踪模块', value: p.modules, enabled: !isBusy, onChanged: (v) { p.modules = v; controller.scheduleProfileSave(); }),
        TextFieldRow(label: '输出路径', value: p.outputPath, enabled: !isBusy, onChanged: (v) { p.outputPath = v; controller.scheduleProfileSave(); }),
        TextFieldRow(label: '触发点', value: p.triggerPoint, enabled: triggerFieldEnabled, onChanged: (v) { p.triggerPoint = v; controller.scheduleProfileSave(); }),
        TextFieldRow(label: '线程 ID', value: p.threadId, enabled: threadEnabled, onChanged: (v) { p.threadId = v; controller.scheduleProfileSave(); }),
        TextFieldRow(label: '事件上限', value: p.maxEvents, enabled: !isBusy, onChanged: (v) { p.maxEvents = v; controller.scheduleProfileSave(); }),
        DropdownRow(label: '后端', value: p.backend, values: const ['DR', 'TF', 'PT'], enabled: !isBusy, onChanged: (v) => setState(() { p.backend = v; controller.scheduleProfileSave(); })),
      ]),
      Align(alignment: Alignment.centerLeft, child: Wrap(spacing: 12, runSpacing: 8, children: [
        ActionButton(label: '一键加载 Agent', icon: Icons.login, onPressed: controller.canLoadAgent ? () => _run(controller.loadAgent) : null),
        ActionButton(label: '刷新模块', icon: Icons.refresh, onPressed: controller.canRefreshModules ? () => _run(controller.refreshModules) : null),
      ])),
    ]))]);
  }

  Widget _buildPolicyPage(TraceProfile p, bool isBusy, bool triggerOn, bool captureOn)
  {
    final blockMainEnabled = !isBusy && triggerOn && captureOn;
    return PagePad(children: [SectionCard(title: 'Run Policy', subtitle: '运行行为开关，直接影响 configure 参数。', child: FieldGrid(children: [
      SwitchRow(label: '自动线程捕获', value: p.threadCapture, enabled: !isBusy, onChanged: (v) => setState(() { p.threadCapture = v; controller.scheduleProfileSave(); })),
      SwitchRow(label: '定点触发', value: p.triggerEnabled, enabled: !isBusy, onChanged: (v) => setState(() { p.triggerEnabled = v; controller.scheduleProfileSave(); })),
      SwitchRow(label: '阻塞主线程', value: p.blockMainThread, enabled: blockMainEnabled, onChanged: (v) => setState(() { p.blockMainThread = v; controller.scheduleProfileSave(); })),
      SwitchRow(label: '记录模块外', value: p.traceOutsideModules, enabled: !isBusy, onChanged: (v) => setState(() { p.traceOutsideModules = v; controller.scheduleProfileSave(); })),
      SwitchRow(label: '重复命中', value: p.repeatHits, enabled: !isBusy, onChanged: (v) => setState(() { p.repeatHits = v; controller.scheduleProfileSave(); })),
      SwitchRow(label: '增强采样', value: p.enhancedSampling, enabled: !isBusy, onChanged: (v) => setState(() { p.enhancedSampling = v; controller.scheduleProfileSave(); })),
      SwitchRow(label: '根返回停止', value: p.rootStopOnReturn, enabled: !isBusy, onChanged: (v) => setState(() { p.rootStopOnReturn = v; controller.scheduleProfileSave(); })),
      SwitchRow(label: '异步线程接力', value: p.asyncThreadHandoff, enabled: !isBusy, onChanged: (v) => setState(() { p.asyncThreadHandoff = v; controller.scheduleProfileSave(); })),
    ]))]);
  }

  Widget _buildDepthPage(TraceProfile p, bool isBusy)
  {
    final outsideEnabled = !isBusy && p.outsideCallDepthEnabled;
    final anonEnabled = !isBusy && p.anonymousExecCallDepthEnabled;
    final idleEnabled = !isBusy && p.idleEscapeEnabled;
    return PagePad(children: [SectionCard(title: 'Depth Filter', subtitle: controller.depthSummary(), child: FieldGrid(children: [
      TextFieldRow(label: '默认层级', value: p.callDepth, enabled: !isBusy, onChanged: (v) { p.callDepth = v; controller.scheduleProfileSave(); }),
      TextFieldRow(label: '模块规则', value: p.moduleCallDepths, enabled: !isBusy, onChanged: (v) { p.moduleCallDepths = v; controller.scheduleProfileSave(); }),
      SwitchRow(label: '模块外规则', value: p.outsideCallDepthEnabled, enabled: !isBusy, onChanged: (v) => setState(() { p.outsideCallDepthEnabled = v; controller.scheduleProfileSave(); })),
      TextFieldRow(label: '模块外层级', value: p.outsideCallDepth, enabled: outsideEnabled, onChanged: (v) { p.outsideCallDepth = v; controller.scheduleProfileSave(); }),
      DropdownRow(label: '模块外模式', value: p.outsideExecutionMode, values: const ['EDGE', 'TF'], enabled: outsideEnabled, onChanged: (v) => setState(() { p.outsideExecutionMode = v; controller.scheduleProfileSave(); })),
      SwitchRow(label: '匿名页规则', value: p.anonymousExecCallDepthEnabled, enabled: !isBusy, onChanged: (v) => setState(() { p.anonymousExecCallDepthEnabled = v; controller.scheduleProfileSave(); })),
      TextFieldRow(label: '匿名页层级', value: p.anonymousExecCallDepth, enabled: anonEnabled, onChanged: (v) { p.anonymousExecCallDepth = v; controller.scheduleProfileSave(); }),
      DropdownRow(label: '匿名页模式', value: p.anonymousExecExecutionMode, values: const ['EDGE', 'TF'], enabled: anonEnabled, onChanged: (v) => setState(() { p.anonymousExecExecutionMode = v; controller.scheduleProfileSave(); })),
      SwitchRow(label: '空转跳出', value: p.idleEscapeEnabled, enabled: !isBusy, onChanged: (v) => setState(() { p.idleEscapeEnabled = v; controller.scheduleProfileSave(); })),
      TextFieldRow(label: '空转阈值', value: p.idleEscapeThreshold, enabled: idleEnabled, onChanged: (v) { p.idleEscapeThreshold = v; controller.scheduleProfileSave(); }),
    ]))]);
  }

  Widget _buildObserverPage(TraceProfile p, bool isBusy)
  {
    final specEnabled = !isBusy && p.probeEnabled;
    return PagePad(children: [SectionCard(title: 'Observer / Probe', subtitle: 'Capture / Step / Write probe 规则。', child: Column(children: [
      SwitchRow(label: '启用观测器', value: p.probeEnabled, enabled: !isBusy, onChanged: (v) => setState(() { p.probeEnabled = v; controller.scheduleProfileSave(); })),
      TextFieldRow(label: '规则', value: p.probeSpec, maxLines: 7, enabled: specEnabled, onChanged: (v) { p.probeSpec = v; controller.scheduleProfileSave(); }),
    ]))]);
  }

  Widget _buildMemoryPage(bool isBusy)
  {
    return PagePad(children: [SectionCard(title: 'Memory Tool', subtitle: '调试时临时读写内存。', child: Column(children: [
      FieldGrid(children: [
        TextFieldRow(label: '地址', value: '', controller: memoryAddressController, enabled: !isBusy, onChanged: (_) {}),
        TextFieldRow(label: '读取长度', value: '64', controller: memorySizeController, enabled: !isBusy, onChanged: (_) {}),
        DropdownRow(label: '写入模式', value: memoryMode, values: const ['HEX', 'TEXT', 'UTF16', 'U32', 'U64'], enabled: !isBusy, onChanged: (v) => setState(() => memoryMode = v)),
        TextFieldRow(label: '写入值', value: '', controller: memoryValueController, enabled: !isBusy, onChanged: (_) {}),
      ]),
      Align(alignment: Alignment.centerLeft, child: Wrap(spacing: 12, children: [
        ActionButton(label: '读取', icon: Icons.download, onPressed: controller.canReadMemory ? () => _run(() => controller.readMemory(memoryAddressController.text, memorySizeController.text)) : null),
        ActionButton(label: '写入', icon: Icons.upload, onPressed: controller.canWriteMemory ? () => _run(() => controller.writeMemory(memoryAddressController.text, memoryMode, memoryValueController.text)) : null),
      ])),
      TextPanel(title: '最近内存结果', text: controller.memoryResultText, minHeight: 120),
    ]))]);
  }

  Widget _buildTracePage()
  {
    return PagePad(children: [
      SectionCard(title: 'Trace Control', subtitle: controller.statusText, child: Wrap(spacing: 12, runSpacing: 8, children: [
        ActionButton(label: '开始追踪', icon: Icons.play_arrow, onPressed: controller.canStartTrace ? () => _run(controller.startTrace) : null),
        ActionButton(label: '停止追踪', icon: Icons.stop, onPressed: controller.canStopTrace ? () => _run(controller.stopTrace) : null),
      ])),
      TextPanel(
        title: controller.previewStatus,
        text: controller.previewText,
        minHeight: 430,
        actions: [
          ActionButton(label: '复制全部', icon: Icons.copy, onPressed: controller.previewText.isEmpty ? null : () => _copyText(controller.previewText)),
          ActionButton(label: '地址→内存', icon: Icons.arrow_forward, onPressed: controller.previewText.isEmpty ? null : () => _extractAddressToMemory(controller.previewText)),
        ],
      ),
    ]);
  }

  Widget _buildLogPage()
  {
    final loaderLog = controller.loaderLogs.map((entry) => entry.text).join('\n');
    return PagePad(children: [
      SectionCard(title: 'Log Control', subtitle: '看到异常输出时可就地停止追踪。', child: Align(alignment: Alignment.centerLeft, child: Wrap(spacing: 12, children: [
        ActionButton(label: '停止追踪', icon: Icons.stop, onPressed: controller.canStopTrace ? () => _run(controller.stopTrace) : null),
        ActionButton(label: '清空日志', icon: Icons.delete_sweep, onPressed: () => setState(() => controller.clearLog())),
      ]))),
      TextPanel(
        title: '动作日志',
        text: controller.outputLog,
        minHeight: 420,
        actions: [
          ActionButton(label: '复制全部', icon: Icons.copy, onPressed: controller.outputLog.isEmpty ? null : () => _copyText(controller.outputLog)),
          ActionButton(label: '地址→内存', icon: Icons.arrow_forward, onPressed: controller.outputLog.isEmpty ? null : () => _extractAddressToMemory(controller.outputLog)),
        ],
      ),
      TextPanel(title: 'Loader 日志', text: loaderLog, minHeight: 220),
    ]);
  }

  Widget _buildModulePage()
  {
    return PagePad(children: [
      SectionCard(
        title: 'Module Tools',
        subtitle: controller.moduleNames.isEmpty ? '等待 Agent 上线后自动刷新真实模块列表。' : '已发现 ${controller.moduleNames.length} 个真实模块。',
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          ModulePicker(key: const ValueKey('page_module_picker'), controller: controller, controllerText: moduleController, onChanged: _selectDumpModule),
          const SizedBox(height: 10),
          Align(alignment: Alignment.centerLeft, child: Wrap(spacing: 12, runSpacing: 8, children: [
            ActionButton(label: '刷新模块', icon: Icons.refresh, onPressed: controller.canRefreshModules ? () => _run(controller.refreshModules) : null),
            ActionButton(label: 'Dump+Fix', icon: Icons.inventory_2, onPressed: controller.canDumpModule ? () => _run(() => controller.dumpModule(controller.selectedDumpModule ?? '')) : null),
          ])),
        ]),
      ),
      TextPanel(title: '模块列表', text: controller.moduleList, minHeight: 420),
    ]);
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

  void _copyText(String text)
  {
    Clipboard.setData(ClipboardData(text: text));
    ScaffoldMessenger.of(context).showSnackBar(const SnackBar(
      content: Text('已复制到剪贴板'),
      behavior: SnackBarBehavior.floating,
      duration: Duration(seconds: 2),
    ));
  }

  void _extractAddressToMemory(String text)
  {
    final match = RegExp(r'0x[0-9a-fA-F]+').firstMatch(text);
    if (match == null)
    {
      ScaffoldMessenger.of(context).showSnackBar(const SnackBar(
        content: Text('未找到十六进制地址'),
        behavior: SnackBarBehavior.floating,
        duration: Duration(seconds: 2),
      ));
      return;
    }
    memoryAddressController.text = match.group(0)!;
    tabController.animateTo(4);
  }
}
