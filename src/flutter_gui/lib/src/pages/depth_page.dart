import 'package:flutter/material.dart';

import '../theme/app_palette.dart';
import '../vdtrace_controller.dart';
import '../widgets/form_fields.dart';
import '../widgets/section_card.dart';

/// Depth-filter policy: default depth plus per-scope override rules.
class DepthPage extends StatelessWidget
{
  const DepthPage({super.key, required this.controller, required this.onRefresh});

  final VdTraceController controller;
  final VoidCallback onRefresh;

  @override
  Widget build(BuildContext context)
  {
    final profile = controller.profile;
    final busy = controller.busy;
    final outsideEnabled = !busy && profile.outsideCallDepthEnabled;
    final anonEnabled = !busy && profile.anonymousExecCallDepthEnabled;
    final idleEnabled = !busy && profile.idleEscapeEnabled;

    return PageBody(children: [
      SectionCard(
        title: 'Depth Filter',
        subtitle: controller.depthSummary(),
        icon: Icons.account_tree,
        accent: VdColors.violet,
        child: FieldGrid(children: [
          VdTextField(label: '默认层级', icon: Icons.layers, value: profile.callDepth, enabled: !busy, onChanged: (value) { profile.callDepth = value; controller.scheduleProfileSave(); }),
          VdTextField(label: '模块规则', icon: Icons.rule, value: profile.moduleCallDepths, hint: '模块名:层级[:模式]', enabled: !busy, onChanged: (value) { profile.moduleCallDepths = value; controller.scheduleProfileSave(); }),
          VdSwitch(label: '模块外规则', value: profile.outsideCallDepthEnabled, enabled: !busy, onChanged: (value) { profile.outsideCallDepthEnabled = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdTextField(label: '模块外层级', icon: Icons.south_east, value: profile.outsideCallDepth, enabled: outsideEnabled, onChanged: (value) { profile.outsideCallDepth = value; controller.scheduleProfileSave(); }),
          VdDropdown(label: '模块外模式', value: profile.outsideExecutionMode, values: const ['EDGE', 'TF'], enabled: outsideEnabled, onChanged: (value) { profile.outsideExecutionMode = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdSwitch(label: '匿名页规则', value: profile.anonymousExecCallDepthEnabled, enabled: !busy, onChanged: (value) { profile.anonymousExecCallDepthEnabled = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdTextField(label: '匿名页层级', icon: Icons.blur_on, value: profile.anonymousExecCallDepth, enabled: anonEnabled, onChanged: (value) { profile.anonymousExecCallDepth = value; controller.scheduleProfileSave(); }),
          VdDropdown(label: '匿名页模式', value: profile.anonymousExecExecutionMode, values: const ['EDGE', 'TF'], enabled: anonEnabled, onChanged: (value) { profile.anonymousExecExecutionMode = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdSwitch(label: '空转跳出', value: profile.idleEscapeEnabled, enabled: !busy, onChanged: (value) { profile.idleEscapeEnabled = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdTextField(label: '空转阈值', icon: Icons.speed, value: profile.idleEscapeThreshold, enabled: idleEnabled, onChanged: (value) { profile.idleEscapeThreshold = value; controller.scheduleProfileSave(); }),
        ]),
      ),
    ]);
  }
}
