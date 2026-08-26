import 'dart:io';

import 'depth_filters.dart';
import 'models.dart';
import 'trace_profile.dart';

File settingsFile(Directory repoRoot)
{
  return File('${repoRoot.path}${Platform.pathSeparator}vdtrace_gui.ini');
}

TraceProfile loadTraceProfile(Directory repoRoot, String agentPath)
{
  final file = settingsFile(repoRoot);
  if (!file.existsSync())
  {
    final profile = TraceProfile(agentPath: agentPath);
    saveTraceProfile(repoRoot, profile);
    return profile;
  }
  final values = _parseTraceSection(file.readAsStringSync());
  final defaults = TraceProfile(agentPath: agentPath);
  final profile = TraceProfile(
    agentPath: _readText(values, 'agent_path', defaults.agentPath),
    threadId: _readText(values, 'thread_id', defaults.threadId),
    modules: _readText(values, 'modules', defaults.modules),
    outputPath: _readText(values, 'output_path', defaults.outputPath),
    maxEvents: _readText(values, 'max_events', defaults.maxEvents),
    backend: _readText(values, 'backend', defaults.backend).trim().toUpperCase(),
    outsideCallDepth: _readText(values, 'outside_call_depth', defaults.outsideCallDepth),
    outsideExecutionMode: _readText(values, 'outside_execution_mode', defaults.outsideExecutionMode),
    anonymousExecCallDepth: _readText(values, 'anonymous_exec_call_depth', defaults.anonymousExecCallDepth),
    anonymousExecExecutionMode: _readText(values, 'anonymous_exec_execution_mode', defaults.anonymousExecExecutionMode),
    moduleCallDepths: _readText(values, 'module_call_depths', defaults.moduleCallDepths),
    triggerPoint: _readText(values, 'trigger_point', defaults.triggerPoint),
    probeSpec: _readText(values, 'probe_spec', defaults.probeSpec),
    probeEnabled: _readBool(values, 'probe_enabled', defaults.probeEnabled),
    triggerEnabled: _readBool(values, 'trigger_enabled', defaults.triggerEnabled),
    blockMainThread: _readBool(values, 'block_main_thread', defaults.blockMainThread),
    traceOutsideModules: _readBool(values, 'trace_outside_modules', defaults.traceOutsideModules),
    allEvents: _readBool(values, 'all_events', defaults.allEvents),
    repeatHits: _readBool(values, 'repeat_hits', defaults.repeatHits),
    idleEscapeThreshold: _readText(values, 'idle_escape_threshold', defaults.idleEscapeThreshold),
    enhancedSampling: _readBool(values, 'enhanced_sampling', defaults.enhancedSampling),
    rootStopOnReturn: _readBool(values, 'root_stop_on_return', defaults.rootStopOnReturn),
    asyncThreadHandoff: _readBool(values, 'async_thread_handoff', defaults.asyncThreadHandoff),
  );

  final callDepthMode = _readText(values, 'call_depth_mode', '').trim().toLowerCase();
  final storedCallDepth = _readText(values, 'call_depth', defaults.callDepth);
  profile.callDepth = callDepthMode == 'ui_v2' ? storedCallDepth : _legacyCallDepthToUi(storedCallDepth, defaults.callDepth);
  profile.outsideExecutionMode = _safeMode(profile.outsideExecutionMode, defaults.outsideExecutionMode);
  profile.outsideCallDepthEnabled = values.containsKey('outside_call_depth_enabled')
      ? _readBool(values, 'outside_call_depth_enabled', defaults.outsideCallDepthEnabled)
      : values.containsKey('outside_call_depth') && profile.outsideCallDepth.trim().isNotEmpty;
  profile.anonymousExecExecutionMode = _safeMode(profile.anonymousExecExecutionMode, defaults.anonymousExecExecutionMode);
  profile.anonymousExecCallDepthEnabled = values.containsKey('anonymous_exec_call_depth_enabled')
      ? _readBool(values, 'anonymous_exec_call_depth_enabled', defaults.anonymousExecCallDepthEnabled)
      : values.containsKey('anonymous_exec_call_depth') && profile.anonymousExecCallDepth.trim().isNotEmpty;
  profile.idleEscapeEnabled = values.containsKey('idle_escape_enabled') ? _readBool(values, 'idle_escape_enabled', false) : false;

  final threadCaptureMode = _readText(values, 'thread_capture_mode', '').trim().toLowerCase();
  if (threadCaptureMode == 'ui_v3_auto_positive')
  {
    profile.threadCapture = _readBool(values, 'thread_capture', defaults.threadCapture);
  }
  else if (values.containsKey('thread_capture'))
  {
    profile.threadCapture = !_readBool(values, 'thread_capture', defaults.threadCapture);
  }
  else if (values.containsKey('auto_thread_capture'))
  {
    profile.threadCapture = _readBool(values, 'auto_thread_capture', defaults.threadCapture);
  }
  else if (values.containsKey('manual_thread'))
  {
    profile.threadCapture = !_readBool(values, 'manual_thread', !defaults.threadCapture);
  }

  final normalizedBackend = normalizeBackendText(profile.backend);
  profile.backend = normalizedBackend.isEmpty ? (profile.allEvents ? 'TF' : 'DR') : normalizedBackend;
  saveTraceProfile(repoRoot, profile);
  return profile;
}

void saveTraceProfile(Directory repoRoot, TraceProfile profile)
{
  settingsFile(repoRoot).writeAsStringSync(_renderSettings(profile));
}

Map<String, String> _parseTraceSection(String text)
{
  final values = <String, String>{};
  var inTrace = false;
  for (final rawLine in text.split(RegExp(r'\r?\n')))
  {
    final line = rawLine.trim();
    if (line.isEmpty || line.startsWith(';') || line.startsWith('#'))
    {
      continue;
    }
    if (line.startsWith('[') && line.endsWith(']'))
    {
      inTrace = line.substring(1, line.length - 1).trim().toLowerCase() == 'trace';
      continue;
    }
    if (!inTrace || !line.contains('='))
    {
      continue;
    }
    final index = line.indexOf('=');
    values[line.substring(0, index).trim()] = line.substring(index + 1).trim();
  }
  return values;
}

String _renderSettings(TraceProfile profile)
{
  final values = <String, String>{
    'agent_path': profile.agentPath.trim(),
    'thread_id': profile.threadId.trim().isEmpty ? '0' : profile.threadId.trim(),
    'thread_capture_mode': 'ui_v3_auto_positive',
    'thread_capture': _writeBool(profile.threadCapture),
    'modules': profile.modules.trim(),
    'output_path': profile.outputPath.trim(),
    'max_events': profile.maxEvents.trim().isEmpty ? '0' : profile.maxEvents.trim(),
    'backend': normalizeBackendText(profile.backend).isEmpty ? 'DR' : normalizeBackendText(profile.backend),
    'call_depth_mode': 'ui_v2',
    'call_depth': profile.callDepth.trim().isEmpty ? '0' : profile.callDepth.trim(),
    'outside_call_depth_enabled': _writeBool(profile.outsideCallDepthEnabled),
    'outside_call_depth': profile.outsideCallDepth.trim().isEmpty ? '3' : profile.outsideCallDepth.trim(),
    'outside_execution_mode': normalizeExecutionMode(profile.outsideExecutionMode),
    'anonymous_exec_call_depth_enabled': _writeBool(profile.anonymousExecCallDepthEnabled),
    'anonymous_exec_call_depth': profile.anonymousExecCallDepth.trim().isEmpty ? '3' : profile.anonymousExecCallDepth.trim(),
    'anonymous_exec_execution_mode': normalizeExecutionMode(profile.anonymousExecExecutionMode),
    'module_call_depths': profile.moduleCallDepths.trim(),
    'trigger_point': profile.triggerPoint.trim(),
    'probe_spec': profile.probeSpec.trim(),
    'probe_enabled': _writeBool(profile.probeEnabled),
    'trigger_enabled': _writeBool(profile.triggerEnabled),
    'block_main_thread': _writeBool(profile.blockMainThread),
    'trace_outside_modules': _writeBool(profile.traceOutsideModules),
    'all_events': _writeBool((normalizeBackendText(profile.backend).isEmpty ? 'DR' : normalizeBackendText(profile.backend)) == 'TF'),
    'repeat_hits': _writeBool(profile.repeatHits),
    'idle_escape_enabled': _writeBool(profile.idleEscapeEnabled),
    'idle_escape_threshold': profile.idleEscapeThreshold.trim().isEmpty ? '8' : profile.idleEscapeThreshold.trim(),
    'enhanced_sampling': _writeBool(profile.enhancedSampling),
    'root_stop_on_return': _writeBool(profile.rootStopOnReturn),
    'async_thread_handoff': _writeBool(profile.asyncThreadHandoff),
  };
  final lines = <String>[
    '; VD-Trace Flutter control settings',
    '; GUI and CLI share this trace profile.',
    '[trace]',
  ];
  for (final entry in values.entries)
  {
    lines.add('${entry.key} = ${entry.value}');
  }
  return '${lines.join('\n')}\n';
}

String _readText(Map<String, String> values, String key, String fallback)
{
  return values[key] ?? fallback;
}

bool _readBool(Map<String, String> values, String key, bool fallback)
{
  final value = _readText(values, key, fallback ? 'true' : 'false').trim().toLowerCase();
  if (const {'1', 'true', 'yes', 'on'}.contains(value))
  {
    return true;
  }
  if (const {'0', 'false', 'no', 'off'}.contains(value))
  {
    return false;
  }
  return fallback;
}

String _writeBool(bool value)
{
  return value ? 'true' : 'false';
}

String _legacyCallDepthToUi(String text, String fallback)
{
  final lowered = text.trim().toLowerCase();
  if (lowered == 'all')
  {
    return '0';
  }
  if (lowered == 'same' || lowered == 'single')
  {
    return '1';
  }
  final value = int.tryParse(lowered.isEmpty ? '0' : lowered);
  if (value == null)
  {
    return fallback;
  }
  return '${value < 0 ? 0 : value + 1}';
}

String _safeMode(String text, String fallback)
{
  try
  {
    return normalizeExecutionMode(text);
  }
  catch (_)
  {
    return fallback;
  }
}
