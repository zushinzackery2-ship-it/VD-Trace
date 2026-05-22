import 'depth_filters.dart';
import 'models.dart';

String normalizeBackendText(String text)
{
  final backend = text.trim().toUpperCase();
  if (backend.isEmpty)
  {
    return 'DR';
  }
  if (backend == 'DR' || backend == 'TF' || backend == 'PT')
  {
    return backend;
  }
  return '';
}

String validateProbeSpecText(String text)
{
  final stripped = text.trim();
  if (stripped.isEmpty)
  {
    throw FormatException('已启用观测器，但规则为空。');
  }
  for (final raw in stripped.split(';'))
  {
    final rule = raw.trim();
    if (rule.isEmpty)
    {
      continue;
    }
    final lowered = rule.toLowerCase();
    if (lowered.startsWith('step@') || lowered.startsWith('write@'))
    {
      _validateLocalProbeRule(rule, lowered.startsWith('write@'));
      continue;
    }
    _validateCaptureProbeRule(rule);
  }
  return stripped;
}

TraceConfig buildTraceConfigFromProfile(TraceProfile profile)
{
  final backend = normalizeBackendText(profile.backend).isEmpty ? 'DR' : normalizeBackendText(profile.backend);
  if (backend == 'PT')
  {
    throw FormatException('PT 已迁到 backup/pt；当前主线只支持 DR / TF。');
  }
  final triggerEnabled = profile.triggerEnabled;
  final threadCaptureEnabled = profile.threadCapture;
  var threadId = _parseNonNegativeInt(profile.threadId, '线程 ID');
  final maxEvents = _parseNonNegativeInt(profile.maxEvents, '事件上限');
  final idleEscapeThreshold = _parseNonNegativeInt(profile.idleEscapeThreshold, '空转跳出阈值');
  final autoSelectThread = triggerEnabled && threadCaptureEnabled;
  final blockMainThread = autoSelectThread && profile.blockMainThread;
  if (triggerEnabled)
  {
    if (autoSelectThread)
    {
      threadId = 0;
    }
  }
  else if (threadCaptureEnabled)
  {
    threadId = 0;
  }
  final probeSpec = profile.probeEnabled ? validateProbeSpecText(profile.probeSpec) : '';
  final depthFilterSpec = buildDepthFilterSpec(
    profile.outsideCallDepthEnabled,
    profile.outsideCallDepth,
    profile.outsideExecutionMode,
    profile.anonymousExecCallDepthEnabled,
    profile.anonymousExecCallDepth,
    profile.anonymousExecExecutionMode,
    profile.moduleCallDepths,
  );
  return TraceConfig(
    threadId: threadId,
    autoSelectThread: autoSelectThread,
    blockMainThread: blockMainThread,
    modules: profile.modules.trim(),
    outputPath: profile.outputPath.trim(),
    maxEvents: maxEvents,
    traceOutsideModules: profile.traceOutsideModules,
    backend: backend,
    controlFlowOnly: backend != 'TF',
    maxCallDepth: uiDepthToRuntimeText(profile.callDepth),
    depthFilterSpec: depthFilterSpec,
    hitPolicy: profile.repeatHits ? 'every' : 'first',
    hotBypassThreshold: profile.idleEscapeEnabled ? idleEscapeThreshold : 0,
    enhancedSampling: profile.enhancedSampling,
    triggerPoint: triggerEnabled ? profile.triggerPoint.trim() : '',
    probeSpec: probeSpec,
    stopOnRootReturn: profile.rootStopOnReturn,
    asyncThreadHandoff: profile.asyncThreadHandoff,
  );
}

String formatTraceProfile(TraceProfile profile)
{
  final depthFilterSpec = buildDepthFilterSpec(
    profile.outsideCallDepthEnabled,
    profile.outsideCallDepth,
    profile.outsideExecutionMode,
    profile.anonymousExecCallDepthEnabled,
    profile.anonymousExecCallDepth,
    profile.anonymousExecExecutionMode,
    profile.moduleCallDepths,
  );
  return <String>[
    'agent_path=${profile.agentPath}',
    'thread_id=${profile.threadId.trim().isEmpty ? '0' : profile.threadId.trim()}',
    'thread_capture=${profile.threadCapture}',
    'modules=${profile.modules.trim().isEmpty ? '-' : profile.modules.trim()}',
    'output_path=${profile.outputPath.trim().isEmpty ? '(auto)' : profile.outputPath.trim()}',
    'max_events=${profile.maxEvents.trim().isEmpty ? '0' : profile.maxEvents.trim()}',
    'backend=${normalizeBackendText(profile.backend).isEmpty ? 'DR' : normalizeBackendText(profile.backend)}',
    'call_depth=${profile.callDepth.trim().isEmpty ? '0' : profile.callDepth.trim()}',
    'trigger_enabled=${profile.triggerEnabled}',
    'trigger_point=${profile.triggerPoint.trim().isEmpty ? '-' : profile.triggerPoint.trim()}',
    'observer_enabled=${profile.probeEnabled}',
    'observer_spec=${profile.probeSpec.trim().isEmpty ? '-' : profile.probeSpec.trim()}',
    'trace_outside_modules=${profile.traceOutsideModules}',
    'repeat_hits=${profile.repeatHits}',
    'idle_escape_enabled=${profile.idleEscapeEnabled}',
    'idle_escape_threshold=${profile.idleEscapeThreshold.trim().isEmpty ? '32' : profile.idleEscapeThreshold.trim()}',
    'enhanced_sampling=${profile.enhancedSampling}',
    'block_main_thread=${profile.blockMainThread}',
    'root_stop_on_return=${profile.rootStopOnReturn}',
    'async_thread_handoff=${profile.asyncThreadHandoff}',
    'depth_filter_spec=${depthFilterSpec.isEmpty ? '-' : depthFilterSpec}',
  ].join('\n');
}

int _parseNonNegativeInt(String text, String fieldName)
{
  final value = int.parse(text.trim().isEmpty ? '0' : text.trim());
  if (value < 0)
  {
    throw FormatException('$fieldName必须是非负整数。');
  }
  return value;
}

void _validateLocalProbeRule(String rule, bool isWrite)
{
  final tokens = rule.split(RegExp(r'\s+')).where((token) => token.isNotEmpty).toList();
  if (tokens.isEmpty || !tokens.first.contains('@'))
  {
    throw FormatException('观测器规则语法无效。');
  }
  var hasSteps = false;
  var hasWatch = false;
  for (final token in tokens.skip(1))
  {
    if (!token.contains('='))
    {
      throw FormatException('观测器规则语法无效：参数必须使用 key=value。');
    }
    final index = token.indexOf('=');
    final key = token.substring(0, index).trim().toLowerCase();
    final value = token.substring(index + 1).trim();
    if (key == 'steps')
    {
      final parsed = int.parse(value);
      if (parsed <= 0)
      {
        throw FormatException('观测器规则语法无效：steps 不是正整数。');
      }
      hasSteps = true;
    }
    else if (key == 'exit')
    {
      if (!const {'return', 'leave', 'return-or-leave', 'return_or_leave'}.contains(value.toLowerCase()))
      {
        throw FormatException('观测器规则语法无效：exit 只支持 return / leave / return-or-leave。');
      }
    }
    else if (key == 'watch')
    {
      if (!isWrite)
      {
        throw FormatException('step 规则不能带 watch。');
      }
      final watches = value.split('|').map((item) => item.trim()).where((item) => item.isNotEmpty).toList();
      if (watches.isEmpty || watches.length > 4)
      {
        throw FormatException('write 规则需要 1 到 4 个 watch。');
      }
      for (final watch in watches)
      {
        final parts = watch.split(':').map((part) => part.trim()).toList();
        if (parts.length < 2 || parts.length > 3)
        {
          throw FormatException('write watch 语法无效。');
        }
        final size = int.parse(parts[1]);
        if (size <= 0 || size > 32)
        {
          throw FormatException('write watch 大小必须在 1 到 32 之间。');
        }
      }
      hasWatch = true;
    }
    else
    {
      throw FormatException('未知的观测器参数：$key');
    }
  }
  if (!hasSteps)
  {
    throw FormatException('step/write 规则必须显式配置 steps。');
  }
  if (isWrite && !hasWatch)
  {
    throw FormatException('write 规则必须显式配置 watch。');
  }
}

void _validateCaptureProbeRule(String rule)
{
  if (!rule.contains('->'))
  {
    throw FormatException("观测器规则语法无效：capture 规则缺少 '->'。");
  }
  final index = rule.indexOf('->');
  final hitText = rule.substring(0, index).trim();
  final captureText = rule.substring(index + 2).trim();
  if (hitText.isEmpty || captureText.isEmpty)
  {
    throw FormatException('观测器规则语法无效：命中点或 capture 为空。');
  }
  final captures = captureText.split('|').map((item) => item.trim()).where((item) => item.isNotEmpty).toList();
  if (captures.isEmpty)
  {
    throw FormatException('观测器规则语法无效：没有 capture。');
  }
  if (captures.length > 4)
  {
    throw FormatException('单个 capture 规则最多只能配置 4 个 capture。');
  }
  for (final capture in captures)
  {
    final parts = capture.split(':').map((part) => part.trim()).toList();
    if (parts.length < 2)
    {
      throw FormatException('capture 语法无效。');
    }
    final kind = parts.first.toLowerCase();
    if (kind == 'reg')
    {
      if (parts.length > 3)
      {
        throw FormatException('reg capture 语法无效。');
      }
    }
    else if (kind == 'mem' || kind == 'ptr')
    {
      if (parts.length < 3 || parts.length > 4)
      {
        throw FormatException('$kind capture 语法无效。');
      }
    }
    else
    {
      throw FormatException('未知的 capture 类型：${parts.first}');
    }
  }
}
