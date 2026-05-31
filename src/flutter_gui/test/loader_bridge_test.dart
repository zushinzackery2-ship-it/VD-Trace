import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:vdtrace_gui/src/loader_bridge.dart';
import 'package:vdtrace_gui/src/models.dart';
import 'package:vdtrace_gui/src/trace_cli.dart';
import 'package:vdtrace_gui/src/vdtrace_controller.dart';
import 'package:win32/win32.dart';

const int _headerSize = 16;
const int _genericReadWrite = GENERIC_READ | GENERIC_WRITE;
const int _loadDllRequestPayloadSize = loaderMaxPathChars * 2;
const int _agentHelloPayloadSize = loaderMaxPathChars * 2 + 8;
const int _loadDllReplyPayloadSize = 8 + loaderMaxPathChars * 2 + loaderMaxTextChars * 2;

void main()
{
  test('Flutter workflow loads real Agent through Loader IPC and runs modules/dump', () async
  {
    final repoRoot = Directory.current.parent.parent;
    final agentPath = defaultAgentPath(repoRoot);
    final ctlPath = defaultCtlPath(repoRoot);
    if (!File(agentPath).existsSync() || !File(ctlPath).existsSync())
    {
      markTestSkipped('release Agent/CLI binaries are missing');
      return;
    }

    final pipeName = r'\\.\pipe\VDTraceFlutterRealAgentTest-' '${DateTime.now().microsecondsSinceEpoch}';
    final bridge = LoaderBridge(pipeName: pipeName);
    _RealLoaderClient? client;
    int agentModule = 0;

    try
    {
      bridge.start();
      await Future<void>.delayed(const Duration(milliseconds: 25));
      client = await _RealLoaderClient.connect(pipeName, agentPath);
      await client.sendHello();
      final session = await _waitForSession(bridge);
      expect(session.pid, GetCurrentProcessId());

      expect(await bridge.loadAgent(session, agentPath), isTrue);
      agentModule = await client.handleLoadRequest();
      await _waitForLog(bridge, '[加载成功]');

      final cli = TraceCli(ctlPath: ctlPath, workdir: repoRoot);
      expect((await cli.waitUntilOnline(session.pid, 5000)), isTrue);
      final modules = await cli.modules(session.pid, includeSystemModules: true);
      expect(modules.success, isTrue, reason: modules.message);
      expect(modules.message.toLowerCase(), contains('vdtraceagent.dll'));

      final dumpDir = '${repoRoot.path}${Platform.pathSeparator}bin${Platform.pathSeparator}release${Platform.pathSeparator}dump_flutter_real_agent';
      final dumpDirectory = Directory(dumpDir);
      if (dumpDirectory.existsSync())
      {
        dumpDirectory.deleteSync(recursive: true);
      }
      final dump = await cli.dumpModule(session.pid, 'VDTraceAgent.dll', outputDirectory: dumpDir);
      expect(dump.success, isTrue, reason: dump.message);
      expect(File('$dumpDir${Platform.pathSeparator}VDTraceAgent_dump_fix.dll').existsSync(), isTrue);
      expect(File('$dumpDir${Platform.pathSeparator}VDTraceAgent_dump_raw.dll').existsSync(), isTrue);
    }
    finally
    {
      client?.close();
      bridge.stop();
      if (agentModule != 0)
      {
        _stopAgent(agentModule);
      }
    }
  }, skip: !Platform.isWindows);

  test('VdTraceController drives real Loader Agent modules and DumpFix workflow', () async
  {
    final repoRoot = Directory.current.parent.parent;
    final agentPath = defaultAgentPath(repoRoot);
    final ctlPath = defaultCtlPath(repoRoot);
    if (!File(agentPath).existsSync() || !File(ctlPath).existsSync())
    {
      markTestSkipped('release Agent/CLI binaries are missing');
      return;
    }

    final pipeName = r'\\.\pipe\VDTraceFlutterControllerRealAgentTest-' '${DateTime.now().microsecondsSinceEpoch}';
    final bridge = LoaderBridge(pipeName: pipeName);
    final controller = VdTraceController(
      repoRootOverride: repoRoot,
      loaderBridge: bridge,
      cli: TraceCli(ctlPath: ctlPath, workdir: repoRoot),
      profile: TraceProfile(agentPath: agentPath),
    );
    _RealLoaderClient? client;
    int agentModule = 0;

    try
    {
      bridge.start();
      await Future<void>.delayed(const Duration(milliseconds: 25));
      client = await _RealLoaderClient.connect(pipeName, agentPath);
      await client.sendHello();
      await _waitForSession(bridge);
      expect(controller.syncSessions(), isTrue);
      expect(controller.selectedPid, GetCurrentProcessId());
      expect(controller.canLoadAgent, isTrue);
      expect(controller.canDumpModule, isTrue);

      final loadFuture = controller.loadAgent();
      agentModule = await client.handleLoadRequest();
      await loadFuture;
      expect(controller.moduleNames.map((module) => module.toLowerCase()), contains('vdtraceagent.dll'));

      final dumpDir = '${repoRoot.path}${Platform.pathSeparator}bin${Platform.pathSeparator}release${Platform.pathSeparator}dump';
      final dumpDirectory = Directory(dumpDir);
      if (dumpDirectory.existsSync())
      {
        dumpDirectory.deleteSync(recursive: true);
      }
      final previousOutput = controller.outputLog;
      await controller.dumpModule('VDTraceAgent.dll');
      expect(controller.outputLog, isNot(previousOutput));
      expect(File('$dumpDir${Platform.pathSeparator}VDTraceAgent_dump_fix.dll').existsSync(), isTrue);
      expect(File('$dumpDir${Platform.pathSeparator}VDTraceAgent_dump_raw.dll').existsSync(), isTrue);
    }
    finally
    {
      client?.close();
      bridge.stop();
      if (agentModule != 0)
      {
        _stopAgent(agentModule);
      }
    }
  }, skip: !Platform.isWindows);

  test('LoaderBridge discovers Loader session and sends LoadDllRequest', () async
  {
    final pipeName = r'\\.\pipe\VDTraceFlutterLoaderBridgeTest-' '${DateTime.now().microsecondsSinceEpoch}';
    final bridge = LoaderBridge(pipeName: pipeName);
    int client = INVALID_HANDLE_VALUE;

    try
    {
      bridge.start();
      await Future<void>.delayed(const Duration(milliseconds: 25));
      client = await _connectClient(pipeName);

      _writeAll(client, _buildAgentHello(pid: 24680, processPath: r'C:\Targets\TargetProcess.exe'));
      final session = await _waitForSession(bridge);
      expect(session.pid, 24680);
      expect(session.processPath, r'C:\Targets\TargetProcess.exe');
      expect(session.protocolVersion, 1);
      expect(session.featureFlags, 1);

      final agentPath = r'E:\科研\VD-Trace\bin\release\VDTraceAgent.dll';
      expect(await bridge.loadAgent(session, agentPath), isTrue);

      final request = _readMessage(client);
      final requestData = ByteData.sublistView(request);
      expect(requestData.getUint32(0, Endian.little), loaderMagic);
      expect(requestData.getUint32(4, Endian.little), loaderMsgLoadDllRequest);
      expect(requestData.getUint32(8, Endian.little), _headerSize + _loadDllRequestPayloadSize);
      expect(_readUtf16Z(request, _headerSize, loaderMaxPathChars), agentPath);

      _writeAll(client, _buildLoadReply(pid: 24680, dllPath: agentPath, text: 'loaded'));
      await _waitForLog(bridge, '[加载成功]');
      expect(bridge.snapshotLogs().map((entry) => entry.text).join('\n'), contains(agentPath));
    }
    finally
    {
      if (client != INVALID_HANDLE_VALUE)
      {
        CloseHandle(client);
      }
      bridge.stop();
    }
  }, skip: !Platform.isWindows);
}

class _RealLoaderClient
{
  _RealLoaderClient({required this.handle, required this.agentPath});

  final int handle;
  final String agentPath;

  static Future<_RealLoaderClient> connect(String pipeName, String agentPath) async
  {
    final handle = await _connectClient(pipeName);
    return _RealLoaderClient(handle: handle, agentPath: agentPath);
  }

  Future<void> sendHello() async
  {
    final exePath = Platform.resolvedExecutable;
    _writeAll(handle, _buildAgentHello(pid: GetCurrentProcessId(), processPath: exePath));
  }

  Future<int> handleLoadRequest() async
  {
    final pipe = handle;
    final request = await Isolate.run(() => _readMessage(pipe));
    final data = ByteData.sublistView(request);
    expect(data.getUint32(0, Endian.little), loaderMagic);
    expect(data.getUint32(4, Endian.little), loaderMsgLoadDllRequest);
    final dllPath = _readUtf16Z(request, _headerSize, loaderMaxPathChars);
    expect(dllPath, agentPath);

    final module = _loadAndBootstrapAgent(dllPath);
    _writeAll(handle, _buildLoadReply(pid: GetCurrentProcessId(), dllPath: dllPath, text: module == 0 ? 'load failed' : 'loaded'));
    expect(module, isNot(0));
    return module;
  }

  void close()
  {
    if (handle != INVALID_HANDLE_VALUE)
    {
      CloseHandle(handle);
    }
  }
}

int _loadAndBootstrapAgent(String dllPath)
{
  final path = dllPath.toNativeUtf16();
  try
  {
    final module = LoadLibrary(path);
    if (module == 0)
    {
      return 0;
    }
    final bootstrapName = 'vdtrace_loader_bootstrap'.toNativeUtf8();
    try
    {
      final bootstrap = GetProcAddress(module, bootstrapName);
      if (bootstrap == nullptr)
      {
        FreeLibrary(module);
        return 0;
      }
      final fn = bootstrap.cast<NativeFunction<Int32 Function()>>().asFunction<int Function()>();
      if (fn() == 0)
      {
        FreeLibrary(module);
        return 0;
      }
      return module;
    }
    finally
    {
      calloc.free(bootstrapName);
    }
  }
  finally
  {
    calloc.free(path);
  }
}

void _stopAgent(int module)
{
  final stopName = 'vdtrace_agent_request_stop'.toNativeUtf8();
  try
  {
    final stop = GetProcAddress(module, stopName);
    if (stop != nullptr)
    {
      stop.cast<NativeFunction<Void Function()>>().asFunction<void Function()>()();
    }
    FreeLibrary(module);
  }
  finally
  {
    calloc.free(stopName);
  }
}

Future<int> _connectClient(String pipeName) async
{
  final deadline = DateTime.now().add(const Duration(seconds: 3));
  while (DateTime.now().isBefore(deadline))
  {
    final name = pipeName.toNativeUtf16();
    try
    {
      final handle = CreateFile(name, _genericReadWrite, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      if (handle != INVALID_HANDLE_VALUE)
      {
        return handle;
      }
    }
    finally
    {
      calloc.free(name);
    }
    await Future<void>.delayed(const Duration(milliseconds: 15));
  }
  fail('Timed out connecting fake Loader client to $pipeName');
}

Future<LoaderSessionSnapshot> _waitForSession(LoaderBridge bridge) async
{
  final deadline = DateTime.now().add(const Duration(seconds: 3));
  while (DateTime.now().isBefore(deadline))
  {
    final sessions = bridge.snapshotSessions();
    if (sessions.isNotEmpty)
    {
      return sessions.first;
    }
    await Future<void>.delayed(const Duration(milliseconds: 15));
  }
  fail('Timed out waiting for LoaderBridge session');
}

Future<void> _waitForLog(LoaderBridge bridge, String needle) async
{
  final deadline = DateTime.now().add(const Duration(seconds: 3));
  while (DateTime.now().isBefore(deadline))
  {
    if (bridge.snapshotLogs().any((entry) => entry.text.contains(needle)))
    {
      return;
    }
    await Future<void>.delayed(const Duration(milliseconds: 15));
  }
  fail('Timed out waiting for LoaderBridge log: $needle');
}

Uint8List _buildAgentHello({required int pid, required String processPath})
{
  final size = _headerSize + _agentHelloPayloadSize;
  final message = Uint8List(size);
  final data = ByteData.sublistView(message);
  data.setUint32(0, loaderMagic, Endian.little);
  data.setUint32(4, loaderMsgAgentHello, Endian.little);
  data.setUint32(8, size, Endian.little);
  data.setUint32(12, pid, Endian.little);
  _writeUtf16Z(message, _headerSize, loaderMaxPathChars, processPath);
  data.setUint32(_headerSize + loaderMaxPathChars * 2, 1, Endian.little);
  data.setUint32(_headerSize + loaderMaxPathChars * 2 + 4, 1, Endian.little);
  return message;
}

Uint8List _buildLoadReply({required int pid, required String dllPath, required String text})
{
  final size = _headerSize + _loadDllReplyPayloadSize;
  final message = Uint8List(size);
  final data = ByteData.sublistView(message);
  data.setUint32(0, loaderMagic, Endian.little);
  data.setUint32(4, loaderMsgLoadDllReply, Endian.little);
  data.setUint32(8, size, Endian.little);
  data.setUint32(12, pid, Endian.little);
  data.setUint32(_headerSize, 0, Endian.little);
  data.setUint32(_headerSize + 4, 0, Endian.little);
  _writeUtf16Z(message, _headerSize + 8, loaderMaxPathChars, dllPath);
  _writeUtf16Z(message, _headerSize + 8 + loaderMaxPathChars * 2, loaderMaxTextChars, text);
  return message;
}

Uint8List _readMessage(int handle)
{
  final header = _readExact(handle, _headerSize);
  final size = ByteData.sublistView(header).getUint32(8, Endian.little);
  final payload = _readExact(handle, size - _headerSize);
  return Uint8List.fromList([...header, ...payload]);
}

Uint8List _readExact(int handle, int size)
{
  final bytes = Uint8List(size);
  var offset = 0;
  while (offset < size)
  {
    final remaining = size - offset;
    final buffer = calloc<Uint8>(remaining);
    final read = calloc<DWORD>();
    try
    {
      final ok = ReadFile(handle, buffer, remaining, read, nullptr);
      if (ok == 0 || read.value == 0)
      {
        fail('ReadFile failed while reading $size bytes');
      }
      for (var index = 0; index < read.value; index++)
      {
        bytes[offset + index] = buffer[index];
      }
      offset += read.value;
    }
    finally
    {
      calloc.free(buffer);
      calloc.free(read);
    }
  }
  return bytes;
}

void _writeAll(int handle, Uint8List bytes)
{
  var offset = 0;
  while (offset < bytes.length)
  {
    final remaining = bytes.length - offset;
    final buffer = calloc<Uint8>(remaining);
    final written = calloc<DWORD>();
    try
    {
      for (var index = 0; index < remaining; index++)
      {
        buffer[index] = bytes[offset + index];
      }
      final ok = WriteFile(handle, buffer, remaining, written, nullptr);
      if (ok == 0 || written.value == 0)
      {
        fail('WriteFile failed while writing ${bytes.length} bytes');
      }
      offset += written.value;
    }
    finally
    {
      calloc.free(buffer);
      calloc.free(written);
    }
  }
}

void _writeUtf16Z(Uint8List destination, int byteOffset, int maxChars, String value)
{
  final units = value.codeUnits.take(maxChars - 1).toList();
  for (var index = 0; index < units.length; index++)
  {
    final offset = byteOffset + index * 2;
    destination[offset] = units[index] & 0xff;
    destination[offset + 1] = (units[index] >> 8) & 0xff;
  }
}

String _readUtf16Z(Uint8List payload, int byteOffset, int maxChars)
{
  final units = <int>[];
  for (var index = 0; index < maxChars; index++)
  {
    final offset = byteOffset + index * 2;
    if (offset + 1 >= payload.length)
    {
      break;
    }
    final unit = payload[offset] | (payload[offset + 1] << 8);
    if (unit == 0)
    {
      break;
    }
    units.add(unit);
  }
  return String.fromCharCodes(units);
}
