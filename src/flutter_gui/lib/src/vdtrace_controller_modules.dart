part of 'vdtrace_controller.dart';

extension VdTraceModuleOps on VdTraceController
{
  Future<void> refreshModulesIfAgentOnline() async
  {
    if (busy || _moduleRefreshPending)
    {
      return;
    }
    final session = selectedSession;
    if (session == null)
    {
      return;
    }
    _moduleRefreshPending = true;
    try
    {
      final online = await cli.ping(session.pid);
      if (!online.success)
      {
        return;
      }
      await refreshModulesForSession(session, requireAgentOnline: false, silent: true);
    }
    finally
    {
      _moduleRefreshPending = false;
    }
  }

  Future<bool> refreshModulesForSession(
    LoaderSessionSnapshot session, {
    required bool requireAgentOnline,
    bool silent = false,
  }) async
  {
    if (requireAgentOnline && !await _ensureAgentOnline(session))
    {
      return false;
    }
    final result = await cli.modules(session.pid);
    moduleList = result.message;
    if (!result.success)
    {
      if (!silent)
      {
        _append(result.message);
      }
      return false;
    }

    moduleNames = result.message
        .split(RegExp(r'\r?\n'))
        .map((line) => line.trim())
        .where((line) => line.isNotEmpty)
        .fold<List<String>>(<String>[], (modules, module)
        {
          if (!modules.any((existing) => existing.toLowerCase() == module.toLowerCase()))
          {
            modules.add(module);
          }
          return modules;
        })
        .toList();
    if (moduleNames.isEmpty)
    {
      selectedDumpModule = null;
      if (!silent)
      {
        _append('模块列表为空：Agent 没有返回可 Dump 的真实模块。');
      }
      return false;
    }
    if (moduleNames.isNotEmpty && !moduleNames.contains(selectedDumpModule))
    {
      selectedDumpModule = moduleNames.first;
    }
    if (!silent)
    {
      _append('模块列表已刷新：${moduleNames.length} 个模块。');
    }
    return true;
  }

  String selectDumpModuleByName(String requested)
  {
    final trimmed = requested.trim();
    if (trimmed.isNotEmpty && moduleNames.any((module) => module.toLowerCase() == trimmed.toLowerCase()))
    {
      return trimmed;
    }
    final selected = selectedDumpModule?.trim() ?? '';
    if (selected.isNotEmpty && moduleNames.any((module) => module.toLowerCase() == selected.toLowerCase()))
    {
      return selected;
    }
    return moduleNames.isEmpty ? '' : moduleNames.first;
  }
}
