import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../models.dart';
import '../pages/core_page.dart';
import '../pages/depth_page.dart';
import '../pages/log_page.dart';
import '../pages/memory_page.dart';
import '../pages/module_page.dart';
import '../pages/observer_page.dart';
import '../pages/policy_page.dart';
import '../pages/trace_page.dart';
import '../theme/app_palette.dart';
import '../vdtrace_controller.dart';
import 'nav_rail.dart';
import 'target_header.dart';
import 'title_bar.dart';

/// Index of the Memory section, used by the address→memory jump.
const int _memorySectionIndex = 4;

/// Root shell: title bar, target header, navigation rail and the active page.
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
  final moduleController = TextEditingController();
  final memoryAddressController = TextEditingController();
  final memorySizeController = TextEditingController(text: '64');
  final memoryValueController = TextEditingController();
  String memoryMode = 'HEX';
  int selectedIndex = 0;

  @override
  void initState()
  {
    super.initState();
    controller = VdTraceController();
    if (widget.startRuntime)
    {
      controller.start();
    }
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
        backgroundColor: VdColors.danger.withValues(alpha: 0.9),
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
        decoration: const BoxDecoration(gradient: vdCanvasGradient),
        child: SafeArea(
          child: Column(
            children: [
              const TitleBar(),
              TargetHeader(
                controller: controller,
                moduleController: moduleController,
                onModuleChanged: _selectDumpModule,
                onRun: _run,
                onSessionSelected: (LoaderSessionSnapshot session) => setState(() => controller.selectSession(session)),
              ),
              Expanded(
                child: Row(
                  crossAxisAlignment: CrossAxisAlignment.stretch,
                  children: [
                    NavRail(selectedIndex: selectedIndex, onSelected: (index) => setState(() => selectedIndex = index)),
                    Expanded(
                      child: AnimatedSwitcher(
                        duration: const Duration(milliseconds: 180),
                        child: KeyedSubtree(key: ValueKey(selectedIndex), child: _activePage()),
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _activePage()
  {
    switch (selectedIndex)
    {
      case 1:
        return PolicyPage(controller: controller, onRefresh: _refresh);
      case 2:
        return DepthPage(controller: controller, onRefresh: _refresh);
      case 3:
        return ObserverPage(controller: controller, onRefresh: _refresh);
      case 4:
        return MemoryPage(
          controller: controller,
          onRun: _run,
          addressController: memoryAddressController,
          sizeController: memorySizeController,
          valueController: memoryValueController,
          mode: memoryMode,
          onModeChanged: (value) => setState(() => memoryMode = value),
        );
      case 5:
        return TracePage(controller: controller, onRun: _run, onCopy: _copyText, onExtractAddress: _extractAddressToMemory);
      case 6:
        return LogPage(controller: controller, onRun: _run, onClear: () => setState(controller.clearLog), onCopy: _copyText, onExtractAddress: _extractAddressToMemory);
      case 7:
        return ModulePage(controller: controller, onRun: _run, moduleController: moduleController, onModuleChanged: _selectDumpModule);
      default:
        return CorePage(controller: controller, onRun: _run, onRefresh: _refresh);
    }
  }

  void _refresh()
  {
    setState(() {});
  }

  Future<void> _run(Future<void> Function() action) async
  {
    await action();
    if (mounted)
    {
      setState(() {});
    }
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
    setState(() => selectedIndex = _memorySectionIndex);
  }
}
