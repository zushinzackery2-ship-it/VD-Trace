import 'dart:async';
import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:win32/win32.dart';

import 'models.dart';

class LoaderLogEntry
{
  const LoaderLogEntry(this.text);

  final String text;
}

class LoaderBridge
{
  LoaderBridge();

  final _sessions = <LoaderSessionSnapshot>[];
  final _logs = <LoaderLogEntry>[];
  int _nextSessionId = 1;
  bool _running = false;

  List<LoaderSessionSnapshot> snapshotSessions()
  {
    return List.unmodifiable(_sessions.where((session) => session.connected && session.helloReceived));
  }

  List<LoaderLogEntry> snapshotLogs()
  {
    return List.unmodifiable(_logs);
  }

  void start()
  {
    _running = true;
    _logs.add(const LoaderLogEntry('Flutter Loader bridge 已启动。'));
    unawaited(_acceptOnceLoop());
  }

  void stop()
  {
    _running = false;
  }

  Future<bool> loadAgent(LoaderSessionSnapshot session, String agentPath) async
  {
    _logs.add(LoaderLogEntry('Loader bridge 当前会话 ${session.pid} 未持有可复用 pipe，已阻止直接注入 fallback。'));
    _logs.add(LoaderLogEntry('请保持目标 Loader 在线后刷新会话；完整长连接 pipe 会在后续 bridge 层继续承接。'));
    return false;
  }

  Future<void> _acceptOnceLoop() async
  {
    while (_running)
    {
      await Future<void>.delayed(const Duration(milliseconds: 250));
      final server = _createPipeServer();
      if (server == INVALID_HANDLE_VALUE)
      {
        await Future<void>.delayed(const Duration(seconds: 1));
        continue;
      }
      try
      {
        final connected = ConnectNamedPipe(server, nullptr);
        final error = connected == 0 ? GetLastError() : ERROR_SUCCESS;
        if (connected == 0 && error == ERROR_PIPE_LISTENING)
        {
          continue;
        }
        if (connected == 0 && error != ERROR_PIPE_CONNECTED)
        {
          continue;
        }
        final message = _readMessage(server);
        if (message == null)
        {
          continue;
        }
        _handleMessage(message);
      }
      finally
      {
        DisconnectNamedPipe(server);
        CloseHandle(server);
      }
    }
  }

  int _createPipeServer()
  {
    final name = loaderPipeName.toNativeUtf16();
    try
    {
      return CreateNamedPipe(
        name,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT,
        PIPE_UNLIMITED_INSTANCES,
        4096,
        4096,
        0,
        nullptr,
      );
    }
    finally
    {
      calloc.free(name);
    }
  }

  Uint8List? _readMessage(int pipe)
  {
    final buffer = calloc<Uint8>(8192);
    final bytesRead = calloc<DWORD>();
    try
    {
      final ok = ReadFile(pipe, buffer, 8192, bytesRead, nullptr);
      if (ok == 0 || bytesRead.value < 16)
      {
        return null;
      }
      return Uint8List.fromList(List<int>.generate(bytesRead.value, (index) => buffer[index]));
    }
    finally
    {
      calloc.free(buffer);
      calloc.free(bytesRead);
    }
  }

  void _handleMessage(Uint8List message)
  {
    final data = ByteData.sublistView(message);
    final magic = data.getUint32(0, Endian.little);
    final kind = data.getUint32(4, Endian.little);
    final size = data.getUint32(8, Endian.little);
    final pid = data.getUint32(12, Endian.little);
    if (magic != loaderMagic || message.length < 16 + size)
    {
      _logs.add(const LoaderLogEntry('收到无效 Loader 消息。'));
      return;
    }
    final payload = message.sublist(16, 16 + size);
    if (kind == loaderMsgAgentHello)
    {
      final processPath = _readUtf16Z(payload, 8, loaderMaxPathChars);
      final protocolVersion = payload.length >= 4 ? ByteData.sublistView(payload).getUint32(0, Endian.little) : 0;
      final featureFlags = payload.length >= 8 ? ByteData.sublistView(payload).getUint32(4, Endian.little) : 0;
      _sessions.removeWhere((session) => session.pid == pid);
      _sessions.add(LoaderSessionSnapshot(
        sessionId: _nextSessionId++,
        pid: pid,
        processPath: processPath,
        protocolVersion: protocolVersion,
        featureFlags: featureFlags,
        connected: true,
        helloReceived: true,
      ));
      _logs.add(LoaderLogEntry('Loader online: [$pid] $processPath'));
    }
    else if (kind == loaderMsgAgentLog)
    {
      _logs.add(LoaderLogEntry('[$pid] ${_readUtf16Z(payload, 0, loaderMaxTextChars)}'));
    }
  }

  String _readUtf16Z(Uint8List payload, int byteOffset, int maxChars)
  {
    final units = <int>[];
    for (var i = 0; i < maxChars; i++)
    {
      final offset = byteOffset + i * 2;
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
}
