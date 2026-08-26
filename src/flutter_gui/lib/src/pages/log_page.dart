import 'package:flutter/material.dart';

import '../theme/app_palette.dart';
import '../vdtrace_controller.dart';
import '../widgets/action_button.dart';
import '../widgets/console_panel.dart';
import '../widgets/icon_action.dart';
import '../widgets/section_card.dart';

/// Action log + Loader IPC log, with a quick in-place stop control.
class LogPage extends StatelessWidget
{
  const LogPage({
    super.key,
    required this.controller,
    required this.onRun,
    required this.onClear,
    required this.onCopy,
    required this.onExtractAddress,
  });

  final VdTraceController controller;
  final Future<void> Function(Future<void> Function() action) onRun;
  final VoidCallback onClear;
  final void Function(String text) onCopy;
  final void Function(String text) onExtractAddress;

  @override
  Widget build(BuildContext context)
  {
    final loaderLog = controller.loaderLogs.map((entry) => entry.text).join('\n');
    final actionLog = controller.outputLog;

    return PageBody(children: [
      SectionCard(
        title: 'Log Control',
        subtitle: '看到异常输出时可就地停止追踪。',
        icon: Icons.terminal,
        accent: VdColors.warning,
        child: Align(
          alignment: Alignment.centerLeft,
          child: Wrap(spacing: 10, runSpacing: 8, children: [
            ActionButton(label: '停止追踪', tone: ActionTone.danger, icon: Icons.stop, onPressed: controller.canStopTrace ? () => onRun(controller.stopTrace) : null),
            GhostButton(label: '清空日志', icon: Icons.delete_sweep, onPressed: onClear),
          ]),
        ),
      ),
      ConsolePanel(
        title: '动作日志',
        text: actionLog,
        minHeight: 360,
        emptyHint: '操作 Agent 后这里会记录时间戳与结果。',
        actions: [
          IconAction(icon: Icons.copy, tooltip: '复制全部', onPressed: actionLog.isEmpty ? null : () => onCopy(actionLog)),
          IconAction(icon: Icons.arrow_forward, tooltip: '地址→内存', onPressed: actionLog.isEmpty ? null : () => onExtractAddress(actionLog)),
        ],
      ),
      ConsolePanel(title: 'Loader 日志', text: loaderLog, minHeight: 200, accent: VdColors.violet, emptyHint: '等待 Loader IPC 会话与握手。'),
    ]);
  }
}
