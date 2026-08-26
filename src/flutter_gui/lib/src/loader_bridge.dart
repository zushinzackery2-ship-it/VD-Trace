import 'dart:async';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:win32/win32.dart';

import 'loader_message_codec.dart';
import 'loader_pipe_transport.dart';
import 'models.dart';

const int _sendTimeoutMs = 1500;

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
    pokePendingPipe(pipeName);
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
      final handle = state.handle;
      final path = agentPath;
      final ok = await Isolate.run(() => sendLoadDllRequestBlocking(handle, path))
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
      final server = createPipeServer(pipeName);
      if (server == INVALID_HANDLE_VALUE)
      {
        _appendLog('创建 Loader 控制管道失败，1 秒后重试。');
        await Future<void>.delayed(const Duration(seconds: 1));
        continue;
      }

      _pendingServer = server;
      final connected = await Isolate.run(() => connectPipeBlocking(server));
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
        final available = peekPipeAvailable(handle);
        if (available == null)
        {
          break;
        }
        if (available < messageHeaderSize)
        {
          await Future<void>.delayed(const Duration(milliseconds: 15));
          continue;
        }
        final message = await Isolate.run(() => readMessageBlocking(handle));
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

  void _handleMessage(_LoaderSessionState state, Uint8List message)
  {
    final frame = parseLoaderFrame(message);
    final pid = frame.pid;

    if (frame.kind == loaderMsgAgentHello)
    {
      _handleAgentHello(state, pid, frame.payload);
    }
    else if (frame.kind == loaderMsgAgentLog)
    {
      _appendLog('[Loader] pid=$pid ${readAnsiZ(frame.payload, 0, loaderMaxTextChars)}');
    }
    else if (frame.kind == loaderMsgLoadDllReply)
    {
      _handleLoadDllReply(pid, frame.payload);
    }
  }

  void _handleAgentHello(_LoaderSessionState state, int pid, Uint8List payload)
  {
    final hello = parseAgentHello(payload);
    final duplicates = _sessions.values.where((existing) => existing.snapshot.pid == pid && existing.snapshot.sessionId != state.snapshot.sessionId).toList();
    for (final duplicate in duplicates)
    {
      _sessions.remove(duplicate.snapshot.sessionId);
      _closeSessionPipe(duplicate);
    }
    state.snapshot = LoaderSessionSnapshot(
      sessionId: state.snapshot.sessionId,
      pid: pid,
      processPath: hello.processPath,
      protocolVersion: hello.protocolVersion,
      featureFlags: hello.featureFlags,
      connected: true,
      helloReceived: true,
    );
    _appendLog('[已连接] ${state.snapshot.displayName}');
  }

  void _handleLoadDllReply(int pid, Uint8List payload)
  {
    final reply = parseLoadDllReply(payload);
    if (reply == null)
    {
      _appendLog('收到不完整 Loader 加载回复：pid=$pid');
      return;
    }
    if (reply.ok)
    {
      _appendLog('[加载成功] pid=$pid 路径=${reply.path}');
    }
    else
    {
      _appendLog('[加载失败] pid=$pid 错误=${reply.win32Error} 路径=${reply.path} 文本=${reply.text}');
    }
  }

  void _closeSessionPipe(_LoaderSessionState state)
  {
    if (state.closed)
    {
      return;
    }
    state.closed = true;
    closeServerPipe(state.handle);
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
