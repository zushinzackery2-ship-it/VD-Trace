part of 'vdtrace_controller.dart';

const Duration _pollInterval = Duration(milliseconds: 750);

/// Runtime polling lifecycle helpers for [VdTraceController].
///
/// The poll loop is self-scheduling rather than a fixed [Timer.periodic] so a
/// slow `vdtrace_ctl` invocation can never let ticks pile up on top of each
/// other. While a user action holds the agent (`busy`) the loop skips the
/// status/module probes so two clients never hit the same Agent pipe at once.
extension VdTraceLifecycle on VdTraceController
{
  void _scheduleNextPoll({bool immediate = false})
  {
    if (_disposed)
    {
      return;
    }
    _pollTimer?.cancel();
    _pollTimer = Timer(immediate ? Duration.zero : _pollInterval, _pollTick);
  }

  Future<void> _pollTick() async
  {
    if (_disposed || _polling)
    {
      _scheduleNextPoll();
      return;
    }
    _polling = true;
    try
    {
      await refreshRuntime();
    }
    finally
    {
      _polling = false;
      _scheduleNextPoll();
    }
  }

  Future<void> refreshRuntime() async
  {
    syncSessions();
    if (!busy)
    {
      final responsive = await refreshStatus();
      await _syncModulesForAgentState(responsive);
    }
    notify();
  }

  /// Queries Agent status through the CLI. Returns whether the Agent responded,
  /// which doubles as an "is the Agent online" signal for module syncing.
  Future<bool> refreshStatus() async
  {
    final session = selectedSession;
    if (session == null || session.pid <= 0)
    {
      _agentResponsive = false;
      return false;
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
    final previewResult = preview.refresh(
      profile.agentPath,
      profile.outputPath,
      profile.triggerEnabled ? profile.triggerPoint : '',
    );
    previewText = previewResult.text;
    previewStatus = previewResult.status;
    return result.success;
  }

  /// Refreshes the module list only when the Agent transitions offline→online
  /// or when it is online but no modules have been discovered yet, instead of
  /// re-enumerating modules on every poll tick.
  Future<void> _syncModulesForAgentState(bool responsive) async
  {
    if (!responsive)
    {
      _agentResponsive = false;
      return;
    }
    final justCameOnline = !_agentResponsive;
    _agentResponsive = true;
    if (justCameOnline || moduleNames.isEmpty)
    {
      await refreshModulesIfAgentOnline();
    }
  }
}
