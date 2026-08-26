import 'package:flutter/material.dart';

import '../theme/app_palette.dart';
import '../vdtrace_controller.dart';
import '../widgets/action_button.dart';
import '../widgets/form_fields.dart';
import '../widgets/section_card.dart';

/// Core session parameters: the settings changed most often before a run.
class CorePage extends StatelessWidget
{
  const CorePage({super.key, required this.controller, required this.onRun, required this.onRefresh});

  final VdTraceController controller;
  final Future<void> Function(Future<void> Function() action) onRun;
  final VoidCallback onRefresh;

  @override
  Widget build(BuildContext context)
  {
    final profile = controller.profile;
    final busy = controller.busy;
    final triggerOn = profile.triggerEnabled;
    final captureOn = profile.threadCapture;
    final threadEnabled = !busy && !(triggerOn && captureOn);
    final triggerFieldEnabled = !busy && triggerOn;

    return PageBody(children: [
      SectionCard(
        title: 'Core Trace Setup',
        subtitle: '启动前最常调整的参数集中在这里。',
        icon: Icons.tune,
        accent: VdColors.accent,
        child: Column(
          children: [
            FieldGrid(children: [
              VdTextField(label: 'Agent DLL', icon: Icons.extension, value: profile.agentPath, enabled: !busy, onChanged: (value) { profile.agentPath = value; controller.scheduleProfileSave(); }),
              VdTextField(label: '追踪模块', icon: Icons.dns, value: profile.modules, enabled: !busy, onChanged: (value) { profile.modules = value; controller.scheduleProfileSave(); }),
              VdTextField(label: '输出路径', icon: Icons.save_alt, value: profile.outputPath, hint: '留空自动命名', enabled: !busy, onChanged: (value) { profile.outputPath = value; controller.scheduleProfileSave(); }),
              VdTextField(label: '触发点', icon: Icons.my_location, value: profile.triggerPoint, enabled: triggerFieldEnabled, onChanged: (value) { profile.triggerPoint = value; controller.scheduleProfileSave(); }),
              VdTextField(label: '线程 ID', icon: Icons.tag, value: profile.threadId, enabled: threadEnabled, onChanged: (value) { profile.threadId = value; controller.scheduleProfileSave(); }),
              VdTextField(label: '事件上限', icon: Icons.numbers, value: profile.maxEvents, enabled: !busy, onChanged: (value) { profile.maxEvents = value; controller.scheduleProfileSave(); }),
              VdDropdown(label: '后端', icon: Icons.memory, value: profile.backend, values: const ['DR', 'TF', 'PT'], enabled: !busy, onChanged: (value) { profile.backend = value; controller.scheduleProfileSave(); onRefresh(); }),
            ]),
            const SizedBox(height: 4),
            Align(
              alignment: Alignment.centerLeft,
              child: Wrap(spacing: 10, runSpacing: 8, children: [
                ActionButton(label: '一键加载 Agent', icon: Icons.rocket_launch, onPressed: controller.canLoadAgent ? () => onRun(controller.loadAgent) : null),
                GhostButton(label: '刷新模块', icon: Icons.refresh, onPressed: controller.canRefreshModules ? () => onRun(controller.refreshModules) : null),
              ]),
            ),
          ],
        ),
      ),
    ]);
  }
}
