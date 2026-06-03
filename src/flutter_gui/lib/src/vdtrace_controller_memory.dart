part of 'vdtrace_controller.dart';

extension VdTraceMemoryOps on VdTraceController
{
  Future<void> readMemory(String address, String sizeText) async
  {
    if (address.trim().isEmpty)
    {
      lastError = '请填写内存地址。';
      notify();
      return;
    }
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
    if (address.trim().isEmpty)
    {
      lastError = '请填写内存地址。';
      notify();
      return;
    }
    if (value.trim().isEmpty)
    {
      lastError = '请填写写入值。';
      notify();
      return;
    }
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
}
