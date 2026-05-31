import 'dart:async';
import 'dart:ffi';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:win32/win32.dart';

import 'models.dart';

const int _genericReadWrite = GENERIC_READ | GENERIC_WRITE;
const int _messageHeaderSize = 16;
const int _maxMessageSize = 1024 * 1024;
const int _loadDllReplyPayloadSize = 8 + loaderMaxPathChars * 2 + loaderMaxTextChars * 2;
const int _sendTimeoutMs = 1500;
const int _pokeTimeoutMs = 20;

class LoaderLogEntry
{
  const LoaderLogEntry(this.text);

  final String text;
}

class _LoaderSessionState
{
  _LoaderSessionState({required this.snapshot, required this.handle});

  LoaderSessionSnapshot snapshot;
  final int handle;
  bool closed = false;
  bool sending = false;
}

class _SendLoadDllRequestJob
{
  const _SendLoadDllRequestJob({required this.handle, required this.agentPath});

  final int handle;
  final String agentPath;
}

class LoaderBridge
{
  LoaderBridge({this.pipeName = loaderPipeName});

  final String pipeName;

  final _sessions = <int, _LoaderSessionState>{};
  final _logs = <LoaderLogEntry>[];
  int _nextSessionId = 1;
  int _pendingServer = INVALID_HANDLE_VALUE;
  int _closedPendingServer = INVALID_HANDLE_VALUE;
  bool _running = false;

  List<LoaderSessionSnapshot> snapshotSessions()
  {
    final visible = _sessions.values
        .map((state) => state.snapshot)
        .where((session) => session.connected && session.helloReceived)
        .toList()
      ..sort((left, right) => left.pid.compareTo(right.pid));
    return List.unmodifiable(visible);
  }

  List<LoaderLogEntry> snapshotLogs()
  {
    return List.unmodifiable(_logs);
  }

  void start()
  {
    if (_running)
    {
      return;
    }
    _running = true;
    _appendLog('Flutter Loader bridge 已开始监听命名管道。');
    unawaited(_acceptLoop());
  }

  void stop()
  {
    _running = false;
    _pokePendingPipe(pipeName);
    if (_pendingServer != INVALID_HANDLE_VALUE)
    {
      _closedPendingServer = _pendingServer;
      CloseHandle(_closedPendingServer);
      _pendingServer = INVALID_HANDLE_VALUE;
    }
    final states = List<_LoaderSessionState>.from(_sessions.values);
    _sessions.clear();
    for (final state in states)
    {
      _closeSessionPipe(state);
    }
  }

  Future<bool> loadAgent(LoaderSessionSnapshot session, String agentPath) async
  {
    final state = _sessions[session.sessionId];
    if (state == null || !state.snapshot.connected || state.sending)
    {
      _appendLog('Loader 会话不可用，无法发送 Agent 加载请求：pid=${session.pid}');
      return false;
    }

    state.sending = true;
    try
    {
      final job = _SendLoadDllRequestJob(handle: state.handle, agentPath: agentPath);
      final ok = await Isolate.run(() => _sendLoadDllRequestBlocking(job))
          .timeout(const Duration(milliseconds: _sendTimeoutMs), onTimeout: () => false);
      _appendLog(ok
          ? '已发送 Agent 加载请求：pid=${state.snapshot.pid}'
          : '发送 Agent 加载请求失败：pid=${state.snapshot.pid}');
      return ok;
    }
    finally
    {
      state.sending = false;
    }
  }

  Future<void> _acceptLoop() async
  {
    while (_running)
    {
      final server = _createPipeServer();
      if (server == INVALID_HANDLE_VALUE)
      {
        _appendLog('创建 Loader 控制管道失败，1 秒后重试。');
        await Future<void>.delayed(const Duration(seconds: 1));
        continue;
      }

      _pendingServer = server;
      final connected = await Isolate.run(() => _connectPipeBlocking(server));
      final closedByStop = _closedPendingServer == server;
      if (closedByStop)
      {
        _closedPendingServer = INVALID_HANDLE_VALUE;
      }
      if (_pendingServer == server)
      {
        _pendingServer = INVALID_HANDLE_VALUE;
      }
      if (!connected)
      {
        if (!closedByStop)
        {
          CloseHandle(server);
        }
        if (_running)
        {
          await Future<void>.delayed(const Duration(milliseconds: 100));
        }
        continue;
      }

      if (!_running)
      {
        if (!closedByStop)
        {
          CloseHandle(server);
        }
        break;
      }

      final sessionId = _nextSessionId++;
      final state = _LoaderSessionState(snapshot: LoaderSessionSnapshot(sessionId: sessionId), handle: server);
      _sessions[sessionId] = state;
      unawaited(_sessionLoop(state));
      await Future<void>.delayed(Duration.zero);
    }
  }

  Future<void> _sessionLoop(_LoaderSessionState state) async
  {
    try
    {
      while (_running && state.snapshot.connected)
      {
        final handle = state.handle;
        final available = _peekPipeAvailable(handle);
        if (available == null)
        {
          break;
        }
        if (available < _messageHeaderSize)
        {
          await Future<void>.delayed(const Duration(milliseconds: 15));
          continue;
        }
        final message = await Isolate.run(() => _readMessageBlocking(handle));
        if (message == null)
        {
          break;
        }
        _handleMessage(state, message);
      }
    }
    finally
    {
      final snapshot = state.snapshot;
      if (snapshot.helloReceived)
      {
        _appendLog('[已断开] ${snapshot.displayName}');
      }
      state.snapshot = LoaderSessionSnapshot(
        sessionId: snapshot.sessionId,
        pid: snapshot.pid,
        processPath: snapshot.processPath,
        protocolVersion: snapshot.protocolVersion,
        featureFlags: snapshot.featureFlags,
        connected: false,
        helloReceived: snapshot.helloReceived,
      );
      _sessions.remove(snapshot.sessionId);
      _closeSessionPipe(state);
    }
  }

  int _createPipeServer()
  {
    final name = pipeName.toNativeUtf16();
    try
    {
      return CreateNamedPipe(
        name,
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
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

  void _handleMessage(_LoaderSessionState state, Uint8List message)
  {
    final data = ByteData.sublistView(message);
    final kind = data.getUint32(4, Endian.little);
    final size = data.getUint32(8, Endian.little);
    final pid = data.getUint32(12, Endian.little);
    final payload = message.sublist(_messageHeaderSize, size);

    if (kind == loaderMsgAgentHello)
    {
      final processPath = _readUtf16Z(payload, 0, loaderMaxPathChars);
      final metadataOffset = loaderMaxPathChars * 2;
      final protocolVersion = payload.length >= metadataOffset + 4
          ? ByteData.sublistView(payload).getUint32(metadataOffset, Endian.little)
          : 0;
      final featureFlags = payload.length >= metadataOffset + 8
          ? ByteData.sublistView(payload).getUint32(metadataOffset + 4, Endian.little)
          : 0;

      final duplicates = _sessions.values
          .where((existing) => existing.snapshot.pid == pid && existing.snapshot.sessionId != state.snapshot.sessionId)
          .toList();
      for (final duplicate in duplicates)
      {
        _sessions.remove(duplicate.snapshot.sessionId);
        _closeSessionPipe(duplicate);
      }
      state.snapshot = LoaderSessionSnapshot(
        sessionId: state.snapshot.sessionId,
        pid: pid,
        processPath: processPath,
        protocolVersion: protocolVersion,
        featureFlags: featureFlags,
        connected: true,
        helloReceived: true,
      );
      _appendLog('[已连接] ${state.snapshot.displayName}');
    }
    else if (kind == loaderMsgAgentLog)
    {
      _appendLog('[Loader] pid=$pid ${_readAnsiZ(payload, 0, loaderMaxTextChars)}');
    }
    else if (kind == loaderMsgLoadDllReply)
    {
      _handleLoadDllReply(pid, payload);
    }
  }

  void _handleLoadDllReply(int pid, Uint8List payload)
  {
    if (payload.length < _loadDllReplyPayloadSize)
    {
      _appendLog('收到不完整 Loader 加载回复：pid=$pid');
      return;
    }
    final data = ByteData.sublistView(payload);
    final status = data.getUint32(0, Endian.little);
    final win32Error = data.getUint32(4, Endian.little);
    final path = _readUtf16Z(payload, 8, loaderMaxPathChars);
    final text = _readUtf16Z(payload, 8 + loaderMaxPathChars * 2, loaderMaxTextChars);
    if (status == 0)
    {
      _appendLog('[加载成功] pid=$pid 路径=$path');
    }
    else
    {
      _appendLog('[加载失败] pid=$pid 错误=$win32Error 路径=$path 文本=$text');
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

  String _readAnsiZ(Uint8List payload, int byteOffset, int maxChars)
  {
    final bytes = <int>[];
    for (var index = 0; index < maxChars; index++)
    {
      final offset = byteOffset + index;
      if (offset >= payload.length || payload[offset] == 0)
      {
        break;
      }
      bytes.add(payload[offset]);
    }
    return String.fromCharCodes(bytes);
  }

  void _closePipe(int pipe)
  {
    if (pipe == INVALID_HANDLE_VALUE || pipe == 0)
    {
      return;
    }
    FlushFileBuffers(pipe);
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
  }

  void _closeSessionPipe(_LoaderSessionState state)
  {
    if (state.closed)
    {
      return;
    }
    state.closed = true;
    _closePipe(state.handle);
  }

  void _appendLog(String text)
  {
    _logs.add(LoaderLogEntry(text));
    if (_logs.length > 2048)
    {
      _logs.removeRange(0, _logs.length - 2048);
    }
  }
}

void _pokePendingPipe(String pipeName)
{
  final name = pipeName.toNativeUtf16();
  try
  {
    final waited = _waitNamedPipe(name, _pokeTimeoutMs);
    if (waited == 0)
    {
      return;
    }
    final handle = CreateFile(name, _genericReadWrite, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle != INVALID_HANDLE_VALUE)
    {
      CloseHandle(handle);
    }
  }
  finally
  {
    calloc.free(name);
  }
}

int _waitNamedPipe(Pointer<Utf16> pipeName, int timeoutMs)
{
  final kernel32 = DynamicLibrary.open('kernel32.dll');
  final waitNamedPipe = kernel32.lookupFunction<
      Int32 Function(Pointer<Utf16> name, Uint32 timeoutMs),
      int Function(Pointer<Utf16> name, int timeoutMs)>('WaitNamedPipeW');
  return waitNamedPipe(pipeName, timeoutMs);
}

bool _connectPipeBlocking(int pipe)
{
  final connected = ConnectNamedPipe(pipe, nullptr);
  if (connected != 0)
  {
    return true;
  }
  return GetLastError() == ERROR_PIPE_CONNECTED;
}

int? _peekPipeAvailable(int pipe)
{
  final available = calloc<DWORD>();
  try
  {
    final ok = PeekNamedPipe(pipe, nullptr, 0, nullptr, available, nullptr);
    if (ok == 0)
    {
      return null;
    }
    return available.value;
  }
  finally
  {
    calloc.free(available);
  }
}

Uint8List? _readMessageBlocking(int pipe)
{
  final header = _readExactBlocking(pipe, _messageHeaderSize);
  if (header == null)
  {
    return null;
  }

  final headerData = ByteData.sublistView(header);
  final magic = headerData.getUint32(0, Endian.little);
  final size = headerData.getUint32(8, Endian.little);
  if (magic != loaderMagic || size < _messageHeaderSize || size > _maxMessageSize)
  {
    return null;
  }

  final payloadSize = size - _messageHeaderSize;
  final payload = payloadSize == 0 ? Uint8List(0) : _readExactBlocking(pipe, payloadSize);
  if (payload == null)
  {
    return null;
  }
  return Uint8List.fromList([...header, ...payload]);
}

Uint8List? _readExactBlocking(int pipe, int size)
{
  final result = Uint8List(size);
  var offset = 0;
  while (offset < size)
  {
    final chunkSize = size - offset;
    final buffer = calloc<Uint8>(chunkSize);
    final bytesRead = calloc<DWORD>();
    try
    {
      final ok = ReadFile(pipe, buffer, chunkSize, bytesRead, nullptr);
      if (ok == 0 || bytesRead.value == 0)
      {
        return null;
      }
      for (var index = 0; index < bytesRead.value; index++)
      {
        result[offset + index] = buffer[index];
      }
      offset += bytesRead.value;
    }
    finally
    {
      calloc.free(buffer);
      calloc.free(bytesRead);
    }
  }
  return result;
}

bool _sendLoadDllRequestBlocking(_SendLoadDllRequestJob job)
{
  final payloadSize = loaderMaxPathChars * 2;
  final messageSize = _messageHeaderSize + payloadSize;
  final message = Uint8List(messageSize);
  final data = ByteData.sublistView(message);
  data.setUint32(0, loaderMagic, Endian.little);
  data.setUint32(4, loaderMsgLoadDllRequest, Endian.little);
  data.setUint32(8, messageSize, Endian.little);
  data.setUint32(12, GetCurrentProcessId(), Endian.little);
  _writeUtf16Z(message, _messageHeaderSize, loaderMaxPathChars, job.agentPath);
  return _writeAllBlocking(job.handle, message);
}

bool _writeAllBlocking(int pipe, Uint8List bytes)
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
      final ok = WriteFile(pipe, buffer, remaining, written, nullptr);
      if (ok == 0 || written.value == 0)
      {
        return false;
      }
      offset += written.value;
    }
    finally
    {
      calloc.free(buffer);
      calloc.free(written);
    }
  }
  return true;
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
