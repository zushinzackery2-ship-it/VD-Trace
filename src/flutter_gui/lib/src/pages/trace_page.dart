import 'package:flutter/material.dart';

import '../theme/app_palette.dart';
import '../vdtrace_controller.dart';
import '../widgets/action_button.dart';
import '../widgets/console_panel.dart';
import '../widgets/icon_action.dart';
import '../widgets/section_card.dart';

/// Trace lifecycle controls plus a live tail of the output file.
class TracePage extends StatelessWidget
{
  const TracePage({super.key, required this.controller, required this.onRun, required this.onCopy, required this.onExtractAddress});

  final VdTraceController controller;
  final Future<void> Function(Future<void> Function() action) onRun;
  final void Function(String text) onCopy;
  final void Function(String text) onExtractAddress;

  @override
  Widget build(BuildContext context)
  {
    final preview = controller.previewText;
    return PageBody(children: [
      SectionCard(
        title: 'Trace Control',
        subtitle: controller.statusText,
        icon: Icons.play_circle,
        accent: VdColors.success,
        child: Align(
          alignment: Alignment.centerLeft,
          child: Wrap(spacing: 10, runSpacing: 8, children: [
            ActionButton(label: '开始追踪', tone: ActionTone.positive, icon: Icons.play_arrow, onPressed: controller.canStartTrace ? () => onRun(controller.startTrace) : null),
            ActionButton(label: '停止追踪', tone: ActionTone.danger, icon: Icons.stop, onPressed: controller.canStopTrace ? () => onRun(controller.stopTrace) : null),
          ]),
        ),
      ),
      ConsolePanel(
        title: '追踪预览',
        subtitle: controller.previewStatus,
        text: preview,
        minHeight: 420,
        emptyHint: '开始追踪后这里会实时显示输出文件末端。',
        actions: [
          IconAction(icon: Icons.copy, tooltip: '复制全部', onPressed: preview.isEmpty ? null : () => onCopy(preview)),
          IconAction(icon: Icons.arrow_forward, tooltip: '地址→内存', onPressed: preview.isEmpty ? null : () => onExtractAddress(preview)),
        ],
      ),
    ]);
  }
}
