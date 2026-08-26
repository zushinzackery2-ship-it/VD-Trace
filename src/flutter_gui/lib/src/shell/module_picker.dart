import 'package:flutter/material.dart';

import '../theme/app_palette.dart';
import '../vdtrace_controller.dart';

/// Dropdown that lets the user pick a discovered module for Dump+Fix.
///
/// Falls back to an informative placeholder while the target/agent are still
/// being discovered so the control never renders as an empty box.
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
        decoration: const InputDecoration(labelText: 'Dump 模块', prefixIcon: Icon(Icons.inventory_2, size: 18)),
        child: Text(
          controller.hasTarget ? '真实模块列表将自动刷新' : '等待自动发现目标',
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: const TextStyle(color: VdColors.textFaint, fontSize: 13),
        ),
      );
    }
    final selected = modules.contains(controller.selectedDumpModule) ? controller.selectedDumpModule! : modules.first;
    return DropdownButtonFormField<String>(
      initialValue: selected,
      isDense: true,
      dropdownColor: VdColors.surfaceRaised,
      borderRadius: VdTokens.inputRadius,
      icon: const Icon(Icons.expand_more, color: VdColors.textMuted),
      style: const TextStyle(fontSize: 13, color: VdColors.textStrong),
      decoration: const InputDecoration(labelText: 'Dump 模块', prefixIcon: Icon(Icons.inventory_2, size: 18)),
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
