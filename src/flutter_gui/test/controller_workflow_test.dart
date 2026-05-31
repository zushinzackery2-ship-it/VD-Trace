import 'dart:io';

import 'package:flutter_test/flutter_test.dart';
import 'package:vdtrace_gui/src/loader_bridge.dart';
import 'package:vdtrace_gui/src/models.dart';
import 'package:vdtrace_gui/src/trace_cli.dart';
import 'package:vdtrace_gui/src/vdtrace_controller.dart';

void main()
{
  test('controller auto-selects Loader target and Dump+Fix drives Agent/module workflow', () async
  {
    final loader = _FakeLoaderBridge([
      const LoaderSessionSnapshot(sessionId: 1, pid: 300, processPath: r'C:\Game\PlatformProcess.exe', protocolVersion: 1, featureFlags: 1, connected: true, helloReceived: true),
      const LoaderSessionSnapshot(sessionId: 2, pid: 200, processPath: r'C:\Targets\TargetProcess.exe', protocolVersion: 1, featureFlags: 1, connected: true, helloReceived: true),
    ]);
    final cli = _FakeTraceCli();
    final controller = VdTraceController(
      repoRootOverride: Directory.current,
      loaderBridge: loader,
      cli: cli,
      profile: TraceProfile(agentPath: r'E:\科研\VD-Trace\bin\release\VDTraceAgent.dll'),
    );

    expect(controller.syncSessions(), isTrue);
    expect(controller.selectedPid, 200);
    expect(controller.canDumpModule, isTrue);
    expect(controller.autoTargetTitle, contains('TargetProcess.exe'));

    await controller.dumpModule('');

    expect(loader.loadRequests, [r'E:\科研\VD-Trace\bin\release\VDTraceAgent.dll']);
    expect(cli.pingedPids, contains(200));
    expect(cli.modulesPids, [200]);
    expect(cli.dumpRequests, ['200:TargetModule.dll']);
    expect(controller.moduleNames, ['TargetModule.dll', 'RuntimeSupport.dll']);
    expect(controller.selectedDumpModule, 'TargetModule.dll');
    expect(controller.outputLog, contains('dumped TargetModule.dll'));
  });
}

class _FakeLoaderBridge extends LoaderBridge
{
  _FakeLoaderBridge(this.snapshots);

  final List<LoaderSessionSnapshot> snapshots;
  final List<String> loadRequests = [];

  @override
  List<LoaderSessionSnapshot> snapshotSessions()
  {
    final visible = snapshots.where((session) => session.connected && session.helloReceived).toList()
      ..sort((left, right) => left.pid.compareTo(right.pid));
    return visible;
  }

  @override
  Future<bool> loadAgent(LoaderSessionSnapshot session, String agentPath) async
  {
    loadRequests.add(agentPath);
    return true;
  }
}

class _FakeTraceCli extends TraceCli
{
  _FakeTraceCli() : super(ctlPath: r'C:\fake\vdtrace_ctl.exe', workdir: Directory.current);

  final List<int> pingedPids = [];
  final List<int> modulesPids = [];
  final List<String> dumpRequests = [];
  bool online = false;

  @override
  Future<CommandResult> ping(int pid) async
  {
    pingedPids.add(pid);
    return CommandResult(success: online, message: online ? 'pong' : 'offline');
  }

  @override
  Future<bool> waitUntilOnline(int pid, int timeoutMs) async
  {
    online = true;
    return true;
  }

  @override
  Future<CommandResult> modules(int pid, {bool includeSystemModules = false}) async
  {
    modulesPids.add(pid);
    return const CommandResult(success: true, message: 'TargetModule.dll\nRuntimeSupport.dll\nTargetModule.dll');
  }

  @override
  Future<CommandResult> dumpModule(int pid, String moduleName, {String outputDirectory = r'.\dump'}) async
  {
    dumpRequests.add('$pid:$moduleName');
    return CommandResult(success: true, message: 'dumped $moduleName');
  }
}
