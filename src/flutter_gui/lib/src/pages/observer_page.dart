import 'package:flutter/material.dart';

import '../theme/app_palette.dart';
import '../vdtrace_controller.dart';
import '../widgets/form_fields.dart';
import '../widgets/section_card.dart';

/// Observer / probe rules: capture, step and write-watch specifications.
class ObserverPage extends StatelessWidget
{
  const ObserverPage({super.key, required this.controller, required this.onRefresh});

  final VdTraceController controller;
  final VoidCallback onRefresh;

  @override
  Widget build(BuildContext context)
  {
    final profile = controller.profile;
    final busy = controller.busy;
    final specEnabled = !busy && profile.probeEnabled;

    return PageBody(children: [
      SectionCard(
        title: 'Observer / Probe',
        subtitle: 'Capture / Step / Write probe 规则。',
        icon: Icons.radar,
        accent: VdColors.cyan,
        child: Column(
          children: [
            VdSwitch(label: '启用观测器', subtitle: '开启后按下方规则采集寄存器/内存', value: profile.probeEnabled, enabled: !busy, onChanged: (value) { profile.probeEnabled = value; controller.scheduleProfileSave(); onRefresh(); }),
            const SizedBox(height: 4),
            VdTextField(
              label: '规则',
              value: profile.probeSpec,
              maxLines: 7,
              enabled: specEnabled,
              hint: 'addr -> reg:rax | mem:rsp+8:16 ; step@addr steps=32',
              onChanged: (value) { profile.probeSpec = value; controller.scheduleProfileSave(); },
            ),
          ],
        ),
      ),
    ]);
  }
}
