import 'package:flutter/material.dart';

import '../models.dart';
import '../theme/app_palette.dart';
import '../vdtrace_controller.dart';
import '../widgets/action_button.dart';
import '../widgets/status_chip.dart';
import 'module_picker.dart';

/// Command header: live target/session state plus the primary agent actions.
class TargetHeader extends StatelessWidget
{
  const TargetHeader({
    super.key,
    required this.controller,
    required this.moduleController,
    required this.onModuleChanged,
    required this.onRun,
    required this.onSessionSelected,
  });

  final VdTraceController controller;
  final TextEditingController moduleController;
  final void Function(String value) onModuleChanged;
  final Future<void> Function(Future<void> Function() action) onRun;
  final void Function(LoaderSessionSnapshot session) onSessionSelected;

  @override
  Widget build(BuildContext context)
  {
    return Container(
      padding: const EdgeInsets.fromLTRB(18, 12, 18, 12),
      decoration: const BoxDecoration(
        color: VdColors.canvasTop,
        border: Border(bottom: BorderSide(color: VdColors.lineSoft)),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              Expanded(child: _TargetPanel(controller: controller, onSessionSelected: onSessionSelected)),
              const SizedBox(width: 12),
              _StatusChips(controller: controller),
            ],
          ),
          const SizedBox(height: 12),
          Row(
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              SizedBox(
                width: 280,
                child: ModulePicker(
                  key: const ValueKey('top_module_picker'),
                  controller: controller,
                  controllerText: moduleController,
                  onChanged: onModuleChanged,
                ),
              ),
              const SizedBox(width: 12),
              Expanded(
                child: Wrap(
                  spacing: 10,
                  runSpacing: 8,
                  children: [
                    ActionButton(label: '一键加载 Agent', icon: Icons.rocket_launch, onPressed: controller.canLoadAgent ? () => onRun(controller.loadAgent) : null),
                    ActionButton(label: '刷新模块', icon: Icons.refresh, onPressed: controller.canRefreshModules ? () => onRun(controller.refreshModules) : null),
                    ActionButton(label: 'Dump+Fix', icon: Icons.inventory_2, onPressed: controller.canDumpModule ? () => onRun(() => controller.dumpModule(controller.selectedDumpModule ?? '')) : null),
                  ],
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _TargetPanel extends StatelessWidget
{
  const _TargetPanel({required this.controller, required this.onSessionSelected});

  final VdTraceController controller;
  final void Function(LoaderSessionSnapshot session) onSessionSelected;

  @override
  Widget build(BuildContext context)
  {
    final session = controller.selectedSession;
    final sessions = controller.sessions;
    final online = session != null;

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
      decoration: BoxDecoration(
        color: VdColors.surface,
        borderRadius: VdTokens.inputRadius,
        border: Border.all(color: online ? VdColors.cyan.withValues(alpha: 0.4) : VdColors.line),
      ),
      child: Row(
        children: [
          Icon(online ? Icons.memory : Icons.radar, size: 18, color: online ? VdColors.cyan : VdColors.textFaint),
          const SizedBox(width: 12),
          Expanded(
            child: sessions.length > 1
                ? _SessionSelector(controller: controller, onSessionSelected: onSessionSelected)
                : Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(controller.autoTargetTitle, maxLines: 1, overflow: TextOverflow.ellipsis, style: const TextStyle(fontSize: 13, fontWeight: FontWeight.w700, color: VdColors.textStrong)),
                      const SizedBox(height: 2),
                      Text(controller.statusText, maxLines: 1, overflow: TextOverflow.ellipsis, style: const TextStyle(fontSize: 11.5, color: VdColors.textMuted)),
                    ],
                  ),
          ),
        ],
      ),
    );
  }
}

class _SessionSelector extends StatelessWidget
{
  const _SessionSelector({required this.controller, required this.onSessionSelected});

  final VdTraceController controller;
  final void Function(LoaderSessionSnapshot session) onSessionSelected;

  @override
  Widget build(BuildContext context)
  {
    final sessions = controller.sessions;
    return DropdownButtonHideUnderline(
      child: DropdownButton<int>(
        value: controller.selectedPid,
        isExpanded: true,
        isDense: true,
        dropdownColor: VdColors.surfaceRaised,
        borderRadius: VdTokens.inputRadius,
        icon: const Icon(Icons.expand_more, color: VdColors.textMuted),
        style: const TextStyle(fontWeight: FontWeight.w700, color: VdColors.textStrong, fontSize: 13),
        items: [
          for (final session in sessions)
            DropdownMenuItem(value: session.pid, child: Text(session.displayName, overflow: TextOverflow.ellipsis)),
        ],
        onChanged: (pid)
        {
          if (pid == null)
          {
            return;
          }
          final target = sessions.where((session) => session.pid == pid).firstOrNull;
          if (target != null)
          {
            onSessionSelected(target);
          }
        },
      ),
    );
  }
}

class _StatusChips extends StatelessWidget
{
  const _StatusChips({required this.controller});

  final VdTraceController controller;

  @override
  Widget build(BuildContext context)
  {
    final running = controller.traceRunning;
    final writing = controller.traceWriting;
    return Wrap(
      spacing: 8,
      runSpacing: 8,
      alignment: WrapAlignment.end,
      children: [
        StatChip(label: 'PID', value: '${controller.selectedPid ?? '-'}', color: VdColors.accent),
        StatChip(label: 'TRACE', value: running ? 'RUNNING' : 'IDLE', color: running ? VdColors.success : VdColors.textFaint, pulse: running),
        StatChip(label: 'WRITE', value: writing ? 'BUSY' : 'READY', color: writing ? VdColors.warning : VdColors.textFaint, pulse: writing),
        StatChip(label: 'BACKEND', value: controller.profile.backend, color: VdColors.violet),
        StatChip(label: 'SESSIONS', value: '${controller.sessions.length}', color: VdColors.cyan),
      ],
    );
  }
}
