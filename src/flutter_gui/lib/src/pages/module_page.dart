import 'package:flutter/material.dart';

import '../shell/module_picker.dart';
import '../theme/app_palette.dart';
import '../vdtrace_controller.dart';
import '../widgets/action_button.dart';
import '../widgets/console_panel.dart';
import '../widgets/section_card.dart';

/// Module discovery and Dump+Fix workflow.
class ModulePage extends StatelessWidget
{
  const ModulePage({super.key, required this.controller, required this.onRun, required this.moduleController, required this.onModuleChanged});

  final VdTraceController controller;
  final Future<void> Function(Future<void> Function() action) onRun;
  final TextEditingController moduleController;
  final void Function(String value) onModuleChanged;

  @override
  Widget build(BuildContext context)
  {
    final count = controller.moduleNames.length;
    return PageBody(children: [
      SectionCard(
        title: 'Module Tools',
        subtitle: count == 0 ? '等待 Agent 上线后自动刷新真实模块列表。' : '已发现 $count 个真实模块。',
        icon: Icons.view_list,
        accent: VdColors.cyan,
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            ModulePicker(
              key: const ValueKey('page_module_picker'),
              controller: controller,
              controllerText: moduleController,
              onChanged: onModuleChanged,
            ),
            const SizedBox(height: 10),
            Align(
              alignment: Alignment.centerLeft,
              child: Wrap(spacing: 10, runSpacing: 8, children: [
                ActionButton(label: '刷新模块', icon: Icons.refresh, onPressed: controller.canRefreshModules ? () => onRun(controller.refreshModules) : null),
                ActionButton(label: 'Dump+Fix', icon: Icons.inventory_2, onPressed: controller.canDumpModule ? () => onRun(() => controller.dumpModule(controller.selectedDumpModule ?? '')) : null),
              ]),
            ),
          ],
        ),
      ),
      ConsolePanel(title: '模块列表', text: controller.moduleList, minHeight: 400, emptyHint: '刷新后显示目标进程的真实模块。'),
    ]);
  }
}
