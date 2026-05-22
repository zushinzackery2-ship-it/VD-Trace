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
  VdTraceController()
      : repoRoot = repoRootFromExecutableContext(),
        loaderBridge = LoaderBridge(),
        preview = TracePreviewBuffer()
  {
    profile = loadTraceProfile(repoRoot, defaultAgentPath(repoRoot));
    ctlPath = defaultCtlPath(repoRoot);
    cli = TraceCli(ctlPath: ctlPath, workdir: repoRoot);
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
  String memoryResultText = '';
  bool busy = false;
  bool traceRunning = false;
  bool traceWriting = false;
  Timer? _pollTimer;

  List<LoaderSessionSnapshot> get sessions => loaderBridge.snapshotSessions();
  List<LoaderLogEntry> get loaderLogs => loaderBridge.snapshotLogs();
  bool get hasPid => selectedPid != null && selectedPid! > 0;
  bool get canLoadAgent => hasPid && !busy && !traceRunning;
  bool get canRefreshModules => hasPid && !busy;
  bool get canDumpModule => hasPid && !busy;
  bool get canStartTrace => hasPid && !busy && !traceRunning && !traceWriting;
  bool get canStopTrace => hasPid && !busy && (traceRunning || traceWriting);
  bool get canReadMemory => hasPid && !busy;
  bool get canWriteMemory => hasPid && !busy;

  void start()
  {
    loaderBridge.start();
    _pollTimer = Timer.periodic(const Duration(milliseconds: 750), (_) => unawaited(refreshStatus()));
  }

  void dispose()
  {
    _pollTimer?.cancel();
    loaderBridge.stop();
    saveTraceProfile(repoRoot, profile);
  }

  void selectSession(LoaderSessionSnapshot session)
  {
    selectedPid = session.pid;
    if (isAutoOutputPath(profile.outputPath, session.pid))
    {
      profile.outputPath = '';
    }
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
    final pid = selectedPid;
    if (pid == null || pid <= 0)
    {
      return;
    }
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
    final pid = _requirePid();
    await _runAction(() async
    {
      final online = await cli.ping(pid);
      if (online.success)
      {
        _append('Agent 已在线：${online.message}');
        return;
      }
      final session = sessions.where((item) => item.pid == pid).firstOrNull;
      if (session == null)
      {
        _append('没有找到 PID $pid 的 Loader 会话，未执行直接注入 fallback。');
        return;
      }
      final loaded = await loaderBridge.loadAgent(session, profile.agentPath);
      if (!loaded)
      {
        _append('Loader 加载请求未完成：Flutter bridge 当前只接受会话和日志，未启用直接注入。');
        return;
      }
      final ready = await cli.waitUntilOnline(pid, 5000);
      _append(ready ? 'Agent IPC 已上线。' : '等待 Agent IPC 上线超时。');
    });
  }

  Future<void> refreshModules() async
  {
    final pid = _requirePid();
    await _runAction(() async
    {
      final result = await cli.modules(pid);
      moduleList = result.message;
      _append(result.success ? '模块列表已刷新。' : result.message);
    });
  }

  Future<void> dumpModule(String moduleName) async
  {
    final pid = _requirePid();
    await _runAction(() async
    {
      final result = await cli.dumpModule(pid, moduleName.trim().isEmpty ? profile.modules.trim() : moduleName.trim());
      _append(result.message);
    });
  }

  Future<void> startTrace() async
  {
    final pid = _requirePid();
    await _runAction(() async
    {
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
    final pid = _requirePid();
    await _runAction(() async
    {
      final result = await cli.stop(pid);
      _append(result.message);
      await refreshStatus();
    });
  }

  Future<void> readMemory(String address, String sizeText) async
  {
    final pid = _requirePid();
    await _runAction(() async
    {
      final size = parseMemorySize(sizeText);
      final result = await cli.readMemory(pid, address.trim(), size);
      memoryResultText = result.message;
      _append(result.message);
    });
  }

  Future<void> writeMemory(String address, String mode, String value) async
  {
    final pid = _requirePid();
    await _runAction(() async
    {
      final data = encodeMemoryWriteBytes(mode, value);
      final result = await cli.writeMemory(pid, address.trim(), data);
      memoryResultText = result.message;
      _append(result.message);
    });
  }

  int _requirePid()
  {
    final pid = selectedPid;
    if (pid == null || pid <= 0)
    {
      throw StateError('请先选择 Loader 会话或填写 PID。');
    }
    return pid;
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
