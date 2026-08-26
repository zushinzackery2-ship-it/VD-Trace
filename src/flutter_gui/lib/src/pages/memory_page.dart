import 'package:flutter/material.dart';

import '../theme/app_palette.dart';
import '../vdtrace_controller.dart';
import '../widgets/action_button.dart';
import '../widgets/console_panel.dart';
import '../widgets/form_fields.dart';
import '../widgets/section_card.dart';

/// Ad-hoc memory read/write tool used during a debugging session.
class MemoryPage extends StatelessWidget
{
  const MemoryPage({
    super.key,
    required this.controller,
    required this.onRun,
    required this.addressController,
    required this.sizeController,
    required this.valueController,
    required this.mode,
    required this.onModeChanged,
  });

  final VdTraceController controller;
  final Future<void> Function(Future<void> Function() action) onRun;
  final TextEditingController addressController;
  final TextEditingController sizeController;
  final TextEditingController valueController;
  final String mode;
  final ValueChanged<String> onModeChanged;

  @override
  Widget build(BuildContext context)
  {
    final busy = controller.busy;

    return PageBody(children: [
      SectionCard(
        title: 'Memory Tool',
        subtitle: '调试时临时读写目标进程内存。',
        icon: Icons.grid_view,
        accent: VdColors.accent,
        child: Column(
          children: [
            FieldGrid(children: [
              VdTextField(label: '地址', icon: Icons.place, value: '', controller: addressController, hint: '0x...', enabled: !busy, onChanged: (_) {}),
              VdTextField(label: '读取长度', icon: Icons.straighten, value: '64', controller: sizeController, enabled: !busy, onChanged: (_) {}),
              VdDropdown(label: '写入模式', icon: Icons.edit_note, value: mode, values: const ['HEX', 'TEXT', 'UTF16', 'U32', 'U64'], enabled: !busy, onChanged: onModeChanged),
              VdTextField(label: '写入值', icon: Icons.input, value: '', controller: valueController, enabled: !busy, onChanged: (_) {}),
            ]),
            const SizedBox(height: 4),
            Align(
              alignment: Alignment.centerLeft,
              child: Wrap(spacing: 10, runSpacing: 8, children: [
                ActionButton(label: '读取', icon: Icons.download, onPressed: controller.canReadMemory ? () => onRun(() => controller.readMemory(addressController.text, sizeController.text)) : null),
                ActionButton(label: '写入', tone: ActionTone.danger, icon: Icons.upload, onPressed: controller.canWriteMemory ? () => onRun(() => controller.writeMemory(addressController.text, mode, valueController.text)) : null),
              ]),
            ),
          ],
        ),
      ),
      ConsolePanel(title: '最近内存结果', text: controller.memoryResultText, minHeight: 150, emptyHint: '尚未读取或写入内存。'),
    ]);
  }
}
