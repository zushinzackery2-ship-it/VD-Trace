import 'package:flutter/material.dart';

import '../theme/app_palette.dart';
import '../vdtrace_controller.dart';
import '../widgets/form_fields.dart';
import '../widgets/section_card.dart';

/// Run-behavior toggles that map directly onto `configure` flags.
class PolicyPage extends StatelessWidget
{
  const PolicyPage({super.key, required this.controller, required this.onRefresh});

  final VdTraceController controller;
  final VoidCallback onRefresh;

  @override
  Widget build(BuildContext context)
  {
    final profile = controller.profile;
    final busy = controller.busy;
    final blockMainEnabled = !busy && profile.triggerEnabled && profile.threadCapture;

    return PageBody(children: [
      SectionCard(
        title: 'Run Policy',
        subtitle: '运行行为开关，直接影响 configure 参数。',
        icon: Icons.toggle_on,
        accent: VdColors.cyan,
        child: FieldGrid(children: [
          VdSwitch(label: '自动线程捕获', subtitle: '自动跟随触发线程', value: profile.threadCapture, enabled: !busy, onChanged: (value) { profile.threadCapture = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdSwitch(label: '定点触发', subtitle: '命中触发点后再记录', value: profile.triggerEnabled, enabled: !busy, onChanged: (value) { profile.triggerEnabled = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdSwitch(label: '阻塞主线程', subtitle: '触发+捕获时可用', value: profile.blockMainThread, enabled: blockMainEnabled, onChanged: (value) { profile.blockMainThread = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdSwitch(label: '记录模块外', subtitle: '含指定模块外的执行', value: profile.traceOutsideModules, enabled: !busy, onChanged: (value) { profile.traceOutsideModules = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdSwitch(label: '重复命中', subtitle: '记录每次命中而非首次', value: profile.repeatHits, enabled: !busy, onChanged: (value) { profile.repeatHits = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdSwitch(label: '增强采样', subtitle: '跨模块调用前后预览', value: profile.enhancedSampling, enabled: !busy, onChanged: (value) { profile.enhancedSampling = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdSwitch(label: '根返回停止', subtitle: '根帧返回即结束会话', value: profile.rootStopOnReturn, enabled: !busy, onChanged: (value) { profile.rootStopOnReturn = value; controller.scheduleProfileSave(); onRefresh(); }),
          VdSwitch(label: '异步线程接力', subtitle: '跟随异步派发的线程', value: profile.asyncThreadHandoff, enabled: !busy, onChanged: (value) { profile.asyncThreadHandoff = value; controller.scheduleProfileSave(); onRefresh(); }),
        ]),
      ),
    ]);
  }
}
