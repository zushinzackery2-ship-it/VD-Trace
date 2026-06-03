import 'package:flutter/material.dart';

import 'models.dart';
import 'ui_theme.dart';
import 'ui_widgets.dart';
import 'vdtrace_controller.dart';
import 'window_control.dart';

class TopCommandBar extends StatelessWidget
{
  const TopCommandBar({super.key, required this.controller, required this.moduleController, required this.onModuleChanged, required this.onRun, required this.onSessionSelected});

  final VdTraceController controller;
  final TextEditingController moduleController;
  final void Function(String value) onModuleChanged;
  final Future<void> Function(Future<void> Function() action) onRun;
  final void Function(LoaderSessionSnapshot session) onSessionSelected;

  @override
  Widget build(BuildContext context)
  {
    return Container(
      padding: const EdgeInsets.fromLTRB(14, 10, 12, 10),
      decoration: const BoxDecoration(color: vdPanel, border: Border(bottom: BorderSide(color: vdLine))),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const BrandTitle(),
              const SizedBox(width: 16),
              Expanded(
                child: GestureDetector(
                  behavior: HitTestBehavior.opaque,
                  onPanDown: (_) => startWindowDrag(),
                  onDoubleTap: maximizeOrRestoreWindow,
                  child: const SizedBox(height: 32),
                ),
              ),
              const WindowButtons(),
            ],
          ),
          const SizedBox(height: 10),
          Row(
            children: [
              Expanded(child: _CommandWrap(controller: controller, moduleController: moduleController, onModuleChanged: onModuleChanged, onRun: onRun)),
            ],
          ),
          const SizedBox(height: 8),
          Wrap(
            spacing: 10,
            runSpacing: 10,
            children: [
              StatPill(label: 'PID', value: '${controller.selectedPid ?? '-'}', color: vdBlue),
              StatPill(label: 'TRACE', value: controller.traceRunning ? 'RUNNING' : 'IDLE', color: controller.traceRunning ? vdCyan : vdTextMuted),
              StatPill(label: 'WRITE', value: controller.traceWriting ? 'BUSY' : 'READY', color: controller.traceWriting ? vdAmber : vdTextMuted),
              StatPill(label: 'BACKEND', value: controller.profile.backend, color: vdBlue),
              StatPill(label: 'SESSIONS', value: '${controller.sessions.length}', color: vdCyan),
            ],
          ),
          const SizedBox(height: 6),
          AutoTargetPanel(controller: controller, onSessionSelected: onSessionSelected),
        ],
      ),
    );
  }
}

class BrandTitle extends StatelessWidget
{
  const BrandTitle({super.key});

  @override
  Widget build(BuildContext context)
  {
    return Container(
      key: const ValueKey('vdtrace_brand_title'),
      height: 38,
      padding: const EdgeInsets.fromLTRB(4, 3, 12, 3),
      decoration: BoxDecoration(
        color: vdPanelSoft,
        border: Border.all(color: vdLine),
        borderRadius: BorderRadius.circular(8),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: 30,
            height: 30,
            decoration: BoxDecoration(
              color: vdBlue,
              shape: BoxShape.circle,
              border: Border.all(color: vdCyan.withValues(alpha: 0.55)),
              boxShadow: [BoxShadow(color: vdBlue.withValues(alpha: 0.18), blurRadius: 10, offset: const Offset(0, 2))],
            ),
            child: const Icon(Icons.radar, size: 18, color: Colors.white),
          ),
          const SizedBox(width: 9),
          const Text('VD-Trace', style: TextStyle(fontSize: 21, fontWeight: FontWeight.w900, letterSpacing: 0)),
        ],
      ),
    );
  }
}

class _CommandWrap extends StatelessWidget
{
  const _CommandWrap({required this.controller, required this.moduleController, required this.onModuleChanged, required this.onRun});

  final VdTraceController controller;
  final TextEditingController moduleController;
  final void Function(String value) onModuleChanged;
  final Future<void> Function(Future<void> Function() action) onRun;

  @override
  Widget build(BuildContext context)
  {
    return Wrap(
      spacing: 8,
      runSpacing: 8,
      crossAxisAlignment: WrapCrossAlignment.center,
      children: [
        SizedBox(
          width: 260,
          child: ModulePicker(
            key: const ValueKey('top_module_picker'),
            controller: controller,
            controllerText: moduleController,
            onChanged: onModuleChanged,
          ),
        ),
        ActionButton(label: '一键加载 Agent', icon: Icons.login, onPressed: controller.canLoadAgent ? () => onRun(controller.loadAgent) : null),
        ActionButton(label: '刷新模块', icon: Icons.refresh, onPressed: controller.canRefreshModules ? () => onRun(controller.refreshModules) : null),
        ActionButton(label: 'Dump+Fix', icon: Icons.inventory_2, onPressed: controller.canDumpModule ? () => onRun(() => controller.dumpModule(controller.selectedDumpModule ?? '')) : null),
      ],
    );
  }
}

class AutoTargetPanel extends StatelessWidget
{
  const AutoTargetPanel({super.key, required this.controller, required this.onSessionSelected});

  final VdTraceController controller;
  final void Function(LoaderSessionSnapshot session) onSessionSelected;

  @override
  Widget build(BuildContext context)
  {
    final session = controller.selectedSession;
    final allSessions = controller.sessions;

    if (allSessions.length > 1)
    {
      return Container(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 4),
        decoration: BoxDecoration(color: vdPanelSoft, borderRadius: BorderRadius.circular(8), border: Border.all(color: vdLine)),
        child: DropdownButtonHideUnderline(
          child: DropdownButton<int>(
            value: controller.selectedPid,
            isExpanded: true,
            isDense: true,
            style: const TextStyle(fontWeight: FontWeight.w800, color: vdTextStrong, fontSize: 13),
            items: [
              for (final s in allSessions)
                DropdownMenuItem(value: s.pid, child: Text(s.displayName, overflow: TextOverflow.ellipsis)),
            ],
            onChanged: (pid)
            {
              if (pid == null)
              {
                return;
              }
              final target = allSessions.where((s) => s.pid == pid).firstOrNull;
              if (target != null)
              {
                onSessionSelected(target);
              }
            },
          ),
        ),
      );
    }

    return Row(
      children: [
        Icon(session == null ? Icons.radar : Icons.memory, size: 16, color: session == null ? vdTextMuted : vdCyan),
        const SizedBox(width: 8),
        Expanded(
          child: Text(
            controller.statusText,
            maxLines: 1,
            overflow: TextOverflow.ellipsis,
            style: const TextStyle(color: vdTextStrong, fontWeight: FontWeight.w600, fontSize: 12),
          ),
        ),
      ],
    );
  }
}

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
        decoration: const InputDecoration(labelText: 'Dump 模块', prefixIcon: Icon(Icons.inventory_2)),
        child: Text(
          controller.hasTarget ? '真实模块列表将自动刷新' : '等待自动发现目标',
          maxLines: 1,
          overflow: TextOverflow.ellipsis,
          style: const TextStyle(color: vdTextMuted),
        ),
      );
    }
    final selected = modules.contains(controller.selectedDumpModule) ? controller.selectedDumpModule! : modules.first;
    return DropdownButtonFormField<String>(
      initialValue: selected,
      decoration: const InputDecoration(labelText: 'Dump 模块', prefixIcon: Icon(Icons.inventory_2)),
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

class WindowButtons extends StatelessWidget
{
  const WindowButtons({super.key});

  @override
  Widget build(BuildContext context)
  {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        WindowButton(icon: Icons.remove, tooltip: '最小化', onPressed: minimizeWindow),
        WindowButton(icon: Icons.crop_square, tooltip: '最大化/还原', onPressed: maximizeOrRestoreWindow),
        WindowButton(icon: Icons.close, tooltip: '关闭', danger: true, onPressed: closeWindow),
      ],
    );
  }
}

class WindowButton extends StatelessWidget
{
  const WindowButton({super.key, required this.icon, required this.tooltip, required this.onPressed, this.danger = false});

  final IconData icon;
  final String tooltip;
  final VoidCallback onPressed;
  final bool danger;

  @override
  Widget build(BuildContext context)
  {
    return Tooltip(
      message: tooltip,
      child: InkWell(
        borderRadius: BorderRadius.circular(8),
        onTap: onPressed,
        child: Container(
          width: 34,
          height: 30,
          margin: const EdgeInsets.only(left: 4),
          decoration: BoxDecoration(color: danger ? const Color(0xffffeeee) : vdPanelSoft, borderRadius: BorderRadius.circular(8), border: Border.all(color: vdLine)),
          child: Icon(icon, size: 15, color: danger ? const Color(0xffdc2626) : vdTextStrong),
        ),
      ),
    );
  }
}

class ConfigTabs extends StatelessWidget
{
  const ConfigTabs({super.key, required this.controller});

  final TabController controller;

  @override
  Widget build(BuildContext context)
  {
    return Container(
      margin: const EdgeInsets.fromLTRB(14, 10, 14, 0),
      child: Center(
        child: Container(
          padding: const EdgeInsets.all(5),
          decoration: BoxDecoration(color: vdPanel, border: Border.all(color: vdLine), borderRadius: BorderRadius.circular(14)),
          child: TabBar(
            controller: controller,
            isScrollable: true,
            tabAlignment: TabAlignment.center,
            tabs: [
              Tab(icon: Icon(Icons.tune, size: 16), text: '核心'),
              Tab(icon: Icon(Icons.toggle_on, size: 16), text: '策略'),
              Tab(icon: Icon(Icons.account_tree, size: 16), text: '过滤'),
              Tab(icon: Icon(Icons.radar, size: 16), text: '观测'),
              Tab(icon: Icon(Icons.storage, size: 16), text: '内存'),
              Tab(icon: Icon(Icons.subject, size: 16), text: '预览'),
              Tab(icon: Icon(Icons.terminal, size: 16), text: '日志'),
              Tab(icon: Icon(Icons.view_list, size: 16), text: '模块'),
            ],
          ),
        ),
      ),
    );
  }
}
