import 'dart:async';
import 'dart:io';

import 'depth_filters.dart';
import 'loader_bridge.dart';
import 'memory_codec.dart';
import 'models.dart';
import 'status_formatter.dart';
import 'trace_cli.dart';
import 'trace_preview.dart';
import 'trace_profile.dart';
import 'trace_settings.dart';

class VdTraceController
{
  VdTraceController({Directory? repoRootOverride, LoaderBridge? loaderBridge, TraceCli? cli, TraceProfile? profile})
      : repoRoot = repoRootOverride ?? repoRootFromExecutableContext(),
        loaderBridge = loaderBridge ?? LoaderBridge(),
        preview = TracePreviewBuffer()
  {
    this.profile = profile ?? loadTraceProfile(repoRoot, defaultAgentPath(repoRoot));
    ctlPath = defaultCtlPath(repoRoot);
    this.cli = cli ?? TraceCli(ctlPath: ctlPath, workdir: repoRoot);
  }

  final Directory repoRoot;
  final LoaderBridge loaderBridge;
  final TracePreviewBuffer preview;

  late TraceProfile profile;
  late String ctlPath;
  late TraceCli cli;
  int? selectedPid;
  String statusText = '待机。';
  String previewText = '';
  String previewStatus = '尚未开始预览。';
  String outputLog = '';
  String moduleList = '';
  List<String> moduleNames = [];
  String? selectedDumpModule;
  String memoryResultText = '';
  bool busy = false;
  bool traceRunning = false;
  bool traceWriting = false;
  bool _moduleRefreshPending = false;
  Timer? _pollTimer;

  List<LoaderSessionSnapshot> get sessions => loaderBridge.snapshotSessions();
  List<LoaderLogEntry> get loaderLogs => loaderBridge.snapshotLogs();
  LoaderSessionSnapshot? get selectedSession
  {
    final currentSessions = sessions;
    if (currentSessions.isEmpty)
    {
      return null;
    }
    final pid = selectedPid;
    if (pid != null)
    {
      for (final session in currentSessions)
      {
        if (session.pid == pid)
        {
          return session;
        }
      }
    }
    return currentSessions.first;
  }

  bool get hasTarget => selectedSession != null;
  bool get canLoadAgent => hasTarget && !busy && !traceRunning;
  bool get canRefreshModules => hasTarget && !busy;
  bool get canDumpModule => hasTarget && !busy;
  bool get canStartTrace => hasTarget && !busy && !traceRunning && !traceWriting;
  bool get canStopTrace => hasTarget && !busy && (traceRunning || traceWriting);
  bool get canReadMemory => hasTarget && !busy;
  bool get canWriteMemory => hasTarget && !busy;

  String get autoTargetTitle
  {
    final session = selectedSession;
    if (session == null)
    {
      return '自动发现目标';
    }
    return '自动目标：${session.displayName}';
  }

  String get autoTargetSubtitle
  {
    final count = sessions.length;
    if (count == 0)
    {
      return '等待 Loader IPC 目标自动连接。';
    }
    if (count == 1)
    {
      return '已自动发现 1 个 IPC 目标，无需填写 PID。';
    }
    final session = selectedSession;
    return '已自动发现 $count 个 IPC 目标，当前自动选中 PID ${session?.pid ?? '-'}。';
  }

  void start()
  {
    loaderBridge.start();
    _pollTimer = Timer.periodic(const Duration(milliseconds: 750), (_) => unawaited(refreshRuntime()));
  }

  void dispose()
  {
    _pollTimer?.cancel();
    loaderBridge.stop();
    saveTraceProfile(repoRoot, profile);
  }

  void selectSession(LoaderSessionSnapshot session)
  {
    final previousPid = selectedPid ?? 0;
    selectedPid = session.pid;
    statusText = session.capabilityText;
    if (session.pid != previousPid)
    {
      moduleList = '';
      moduleNames = [];
      selectedDumpModule = null;
      if (isAutoOutputPath(profile.outputPath, previousPid))
      {
        profile.outputPath = '';
      }
    }
  }

  bool syncSessions()
  {
    final currentSessions = sessions;
    if (currentSessions.isEmpty)
    {
      if (selectedPid != null)
      {
        selectedPid = null;
        moduleList = '';
        moduleNames = [];
        selectedDumpModule = null;
        traceRunning = false;
        traceWriting = false;
      }
      statusText = '等待 Loader IPC 会话。';
      return false;
    }

    final current = selectedPid == null
        ? null
        : currentSessions.where((session) => session.pid == selectedPid).firstOrNull;
    if (current == null)
    {
      selectSession(currentSessions.first);
      _append('自动选中 Loader 会话：${currentSessions.first.displayName}');
      return true;
    }
    return false;
  }

  Future<void> refreshRuntime() async
  {
    syncSessions();
    await refreshStatus();
    unawaited(refreshModulesIfAgentOnline());
  }

  String depthSummary()
  {
    return buildDepthFilterSummary(
      profile.callDepth,
      profile.outsideCallDepthEnabled,
      profile.outsideCallDepth,
      profile.outsideExecutionMode,
      profile.anonymousExecCallDepthEnabled,
      profile.anonymousExecCallDepth,
      profile.anonymousExecExecutionMode,
      profile.moduleCallDepths,
      profile.idleEscapeEnabled,
      profile.idleEscapeThreshold,
    );
  }

  String profileSummary()
  {
    return formatTraceProfile(profile);
  }

  Future<void> refreshStatus() async
  {
    final session = selectedSession;
    if (session == null || session.pid <= 0)
    {
      return;
    }
    final pid = session.pid;
    final result = await cli.status(pid);
    if (result.success)
    {
      statusText = formatTraceStatusText(result.message);
      traceRunning = statusMessageIsRunning(result.message);
      traceWriting = statusMessageIsWriting(result.message);
    }
    else
    {
      statusText = result.message;
      traceRunning = false;
      traceWriting = false;
    }
    final previewResult = preview.refresh(profile.agentPath, profile.outputPath, profile.triggerEnabled ? profile.triggerPoint : '');
    previewText = previewResult.text;
    previewStatus = previewResult.status;
  }

  Future<void> loadAgent() async
  {
    final session = _requireSession();
    await _runAction(() async
    {
      if (!await _ensureAgentOnline(session))
      {
        return;
      }
      await _refreshModulesForSession(session, requireAgentOnline: false);
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
      await _refreshModulesForSession(session, requireAgentOnline: false);
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
      await _refreshModulesForSession(session, requireAgentOnline: false);
      final module = _selectDumpModule(moduleName);
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

  Future<void> readMemory(String address, String sizeText) async
  {
    final session = _requireSession();
    await _runAction(() async
    {
      if (!await _ensureAgentOnline(session))
      {
        return;
      }
      final size = parseMemorySize(sizeText);
      final result = await cli.readMemory(session.pid, address.trim(), size);
      memoryResultText = result.message;
      _append(result.message);
    });
  }

  Future<void> writeMemory(String address, String mode, String value) async
  {
    final session = _requireSession();
    await _runAction(() async
    {
      if (!await _ensureAgentOnline(session))
      {
        return;
      }
      final data = encodeMemoryWriteBytes(mode, value);
      final result = await cli.writeMemory(session.pid, address.trim(), data);
      memoryResultText = result.message;
      _append(result.message);
    });
  }

  LoaderSessionSnapshot _requireSession()
  {
    syncSessions();
    final session = selectedSession;
    if (session == null || session.pid <= 0)
    {
      throw StateError('没有发现 Loader IPC 会话。');
    }
    selectedPid = session.pid;
    return session;
  }

  Future<bool> _ensureAgentOnline(LoaderSessionSnapshot session) async
  {
    final online = await cli.ping(session.pid);
    if (online.success)
    {
      _append('Agent 已在线：${online.message}');
      return true;
    }
    final loaded = await loaderBridge.loadAgent(session, profile.agentPath);
    if (!loaded)
    {
      _append('发送 Loader 加载请求失败：${session.displayName}');
      return false;
    }
    final ready = await cli.waitUntilOnline(session.pid, 5000);
    _append(ready ? 'Agent IPC 已上线：${session.displayName}' : '等待 Agent IPC 上线超时：${session.displayName}');
    return ready;
  }

  Future<void> refreshModulesIfAgentOnline() async
  {
    if (busy || _moduleRefreshPending || moduleNames.isNotEmpty)
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
      await _refreshModulesForSession(session, requireAgentOnline: false, silent: true);
    }
    finally
    {
      _moduleRefreshPending = false;
    }
  }

  Future<bool> _refreshModulesForSession(
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

  String _selectDumpModule(String requested)
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

  Future<void> _runAction(Future<void> Function() action) async
  {
    if (busy)
    {
      return;
    }
    busy = true;
    try
    {
      await action();
    }
    catch (error)
    {
      _append('$error');
    }
    finally
    {
      busy = false;
    }
  }

  void _append(String text)
  {
    final stamp = DateTime.now().toIso8601String().substring(11, 19);
    outputLog = '$outputLog[$stamp] $text\n';
  }
}

extension _FirstOrNull<T> on Iterable<T>
{
  T? get firstOrNull
  {
    final iterator = this.iterator;
    if (iterator.moveNext())
    {
      return iterator.current;
    }
    return null;
  }
}
