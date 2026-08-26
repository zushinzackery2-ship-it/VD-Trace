part of 'vdtrace_controller.dart';

/// Agent-facing workflow actions (load, refresh, dump, start/stop trace).
///
/// Every action funnels through [VdTraceController._runAction] so the `busy`
/// flag, error surfacing and listener notification stay in one place.
extension VdTraceWorkflow on VdTraceController
{
  Future<void> loadAgent() async
  {
    final session = _requireSession();
    await _runAction(() async
    {
      if (!await _ensureAgentOnline(session))
      {
        return;
      }
      await refreshModulesForSession(session, requireAgentOnline: false);
    });
  }

  Future<void> refreshModules() async
  {
    final session = _requireSession();
    await _runAction(() async
    {
      if (!await _ensureAgentOnline(session))
      {
        return;
      }
      await refreshModulesForSession(session, requireAgentOnline: false);
    });
  }

  Future<void> dumpModule(String moduleName) async
  {
    final session = _requireSession();
    await _runAction(() async
    {
      if (!await _ensureAgentOnline(session))
      {
        return;
      }
      await refreshModulesForSession(session, requireAgentOnline: false);
      final module = selectDumpModuleByName(moduleName);
      if (module.isEmpty)
      {
        _append('Dump+Fix 失败：Agent 在线但没有返回可用模块。');
        return;
      }
      final result = await cli.dumpModule(session.pid, module);
      _append(result.message);
    });
  }

  Future<void> startTrace() async
  {
    final session = _requireSession();
    await _runAction(() async
    {
      if (!await _ensureAgentOnline(session))
      {
        return;
      }
      final pid = session.pid;
      if (isAutoOutputPath(profile.outputPath, pid))
      {
        profile.outputPath = defaultOutputPath(pid);
      }
      final config = buildTraceConfigFromProfile(profile);
      final configure = await cli.configure(pid, config);
      if (!configure.success)
      {
        _append(configure.message);
        return;
      }
      preview.beginRound(profile.agentPath, profile.outputPath);
      final started = await cli.start(pid);
      _append(started.message);
      saveTraceProfile(repoRoot, profile);
      await refreshStatus();
    });
  }

  Future<void> stopTrace() async
  {
    final session = _requireSession();
    await _runAction(() async
    {
      final result = await cli.stop(session.pid);
      _append(result.message);
      await refreshStatus();
    });
  }
}
