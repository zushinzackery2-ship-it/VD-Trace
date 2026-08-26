import 'dart:io';

const String loaderPipeName = r'\\.\pipe\VDTraceLoaderControl';
const int loaderMagic = 0x58525057;
const int loaderMsgAgentHello = 1;
const int loaderMsgAgentLog = 2;
const int loaderMsgLoadDllRequest = 3;
const int loaderMsgLoadDllReply = 4;
const int loaderMaxPathChars = 1024;
const int loaderMaxTextChars = 256;

class CommandResult
{
  const CommandResult({required this.success, required this.message, this.rawOutput = ''});

  final bool success;
  final String message;
  final String rawOutput;
}

class LoaderSessionSnapshot
{
  const LoaderSessionSnapshot({
    required this.sessionId,
    this.pid = 0,
    this.processPath = '',
    this.protocolVersion = 0,
    this.featureFlags = 0,
    this.connected = true,
    this.helloReceived = false,
  });

  final int sessionId;
  final int pid;
  final String processPath;
  final int protocolVersion;
  final int featureFlags;
  final bool connected;
  final bool helloReceived;

  String get displayName
  {
    if (pid == 0)
    {
      return '[等待握手:$sessionId] Loader 尚未上报';
    }
    return '[$pid] $processPath';
  }

  String get capabilityText
  {
    if (pid == 0)
    {
      return '等待 Loader 握手。';
    }
    return '在线 | 协议 v$protocolVersion';
  }
}

class TraceConfig
{
  const TraceConfig({
    required this.threadId,
    required this.autoSelectThread,
    required this.blockMainThread,
    required this.modules,
    required this.outputPath,
    required this.maxEvents,
    required this.traceOutsideModules,
    required this.backend,
    required this.controlFlowOnly,
    required this.maxCallDepth,
    required this.depthFilterSpec,
    required this.hitPolicy,
    required this.hotBypassThreshold,
    required this.enhancedSampling,
    required this.triggerPoint,
    required this.probeSpec,
    required this.stopOnRootReturn,
    required this.asyncThreadHandoff,
  });

  final int threadId;
  final bool autoSelectThread;
  final bool blockMainThread;
  final String modules;
  final String outputPath;
  final int maxEvents;
  final bool traceOutsideModules;
  final String backend;
  final bool controlFlowOnly;
  final String maxCallDepth;
  final String depthFilterSpec;
  final String hitPolicy;
  final int hotBypassThreshold;
  final bool enhancedSampling;
  final String triggerPoint;
  final String probeSpec;
  final bool stopOnRootReturn;
  final bool asyncThreadHandoff;

  List<String> cliArgs(int pid)
  {
    final args = <String>[
      'configure',
      '$pid',
      '$threadId',
      modules.trim().isEmpty ? '-' : modules.trim(),
      outputPath,
    ];
    if (maxEvents > 0)
    {
      args.add('$maxEvents');
    }
    if (traceOutsideModules)
    {
      args.add('outside');
    }
    args.add('backend=${backend.toLowerCase()}');
    args.add('depth=$maxCallDepth');
    if (depthFilterSpec.isNotEmpty)
    {
      args.add('depthfilter=$depthFilterSpec');
    }
    args.add('hits=$hitPolicy');
    args.add('idleescape=$hotBypassThreshold');
    if (enhancedSampling)
    {
      args.add('sample');
    }
    if (autoSelectThread)
    {
      args.add('autothread');
    }
    if (blockMainThread)
    {
      args.add('blockmain');
    }
    if (triggerPoint.trim().isNotEmpty)
    {
      args.add('trigger=${triggerPoint.trim()}');
    }
    if (probeSpec.trim().isNotEmpty)
    {
      args.add('probe=${probeSpec.trim()}');
    }
    if (stopOnRootReturn)
    {
      args.add('rootstop');
    }
    if (asyncThreadHandoff)
    {
      args.add('handoff');
    }
    return args;
  }
}

class TraceProfile
{
  TraceProfile({
    required this.agentPath,
    this.threadId = '0',
    this.threadCapture = true,
    this.modules = '',
    this.outputPath = '',
    this.maxEvents = '0',
    this.backend = 'DR',
    this.callDepth = '3',
    this.outsideCallDepthEnabled = false,
    this.outsideCallDepth = '3',
    this.outsideExecutionMode = 'EDGE',
    this.anonymousExecCallDepthEnabled = false,
    this.anonymousExecCallDepth = '3',
    this.anonymousExecExecutionMode = 'EDGE',
    this.moduleCallDepths = '',
    this.triggerPoint = '',
    this.probeSpec = '',
    this.probeEnabled = false,
    this.triggerEnabled = false,
    this.blockMainThread = false,
    this.traceOutsideModules = false,
    this.allEvents = false,
    this.repeatHits = false,
    this.idleEscapeEnabled = true,
    this.idleEscapeThreshold = '32',
    this.enhancedSampling = false,
    this.rootStopOnReturn = true,
    this.asyncThreadHandoff = true,
  });

  String agentPath;
  String threadId;
  bool threadCapture;
  String modules;
  String outputPath;
  String maxEvents;
  String backend;
  String callDepth;
  bool outsideCallDepthEnabled;
  String outsideCallDepth;
  String outsideExecutionMode;
  bool anonymousExecCallDepthEnabled;
  String anonymousExecCallDepth;
  String anonymousExecExecutionMode;
  String moduleCallDepths;
  String triggerPoint;
  String probeSpec;
  bool probeEnabled;
  bool triggerEnabled;
  bool blockMainThread;
  bool traceOutsideModules;
  bool allEvents;
  bool repeatHits;
  bool idleEscapeEnabled;
  String idleEscapeThreshold;
  bool enhancedSampling;
  bool rootStopOnReturn;
  bool asyncThreadHandoff;
}

Directory repoRootFromExecutableContext()
{
  var current = Directory.current;
  if (File('${current.path}${Platform.pathSeparator}README.md').existsSync())
  {
    return current;
  }
  var probe = File(Platform.resolvedExecutable).parent;
  for (var i = 0; i < 8; i++)
  {
    if (File('${probe.path}${Platform.pathSeparator}README.md').existsSync())
    {
      return probe;
    }
    final parent = probe.parent;
    if (parent.path == probe.path)
    {
      break;
    }
    probe = parent;
  }
  return current;
}

String defaultAgentPath(Directory repoRoot)
{
  return '${repoRoot.path}${Platform.pathSeparator}bin${Platform.pathSeparator}release${Platform.pathSeparator}VDTraceAgent.dll';
}

String defaultCtlPath(Directory repoRoot)
{
  return '${repoRoot.path}${Platform.pathSeparator}bin${Platform.pathSeparator}release${Platform.pathSeparator}vdtrace_ctl.exe';
}

String defaultOutputPath(int pid)
{
  final now = DateTime.now();
  final stamp = '${now.year.toString().padLeft(4, '0')}${now.month.toString().padLeft(2, '0')}${now.day.toString().padLeft(2, '0')}-${now.hour.toString().padLeft(2, '0')}${now.minute.toString().padLeft(2, '0')}${now.second.toString().padLeft(2, '0')}';
  return r'.\traces\VDTrace-' '$pid-$stamp.log';
}

bool isAutoOutputPath(String text, int pid)
{
  final path = text.trim().replaceAll('/', r'\');
  if (path.isEmpty)
  {
    return true;
  }
  if (RegExp('^\\.\\\\traces\\\\VDTrace\\.log\$', caseSensitive: false).hasMatch(path))
  {
    return true;
  }
  if (RegExp('^\\.\\\\traces\\\\VDTrace-\\d+(?:-\\d{8}-\\d{6})?\\.log\$', caseSensitive: false).hasMatch(path))
  {
    return true;
  }
  if (pid <= 0)
  {
    return false;
  }
  return RegExp('^\\.\\\\traces\\\\VDTrace-$pid(?:-\\d{8}-\\d{6})?\\.log\$', caseSensitive: false).hasMatch(path);
}
