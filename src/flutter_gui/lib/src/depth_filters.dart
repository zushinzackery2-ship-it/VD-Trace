class ModuleDepthRule
{
  const ModuleDepthRule({required this.moduleName, required this.uiDepth, this.executionMode = 'EDGE'});

  final String moduleName;
  final String uiDepth;
  final String executionMode;
}

String normalizeExecutionMode(String text)
{
  final lowered = text.trim().toLowerCase();
  if (lowered == 'tf')
  {
    return 'TF';
  }
  if (lowered.isEmpty || lowered == 'edge')
  {
    return 'EDGE';
  }
  throw FormatException('执行模式只支持 EDGE 或 TF。');
}

String runtimeExecutionModeText(String text)
{
  return normalizeExecutionMode(text).toLowerCase();
}

String normalizeUiDepthValue(String text)
{
  final value = int.parse(text.trim().isEmpty ? '0' : text.trim());
  if (value < 0)
  {
    throw FormatException('追踪层级必须是非负整数。');
  }
  return '$value';
}

String uiDepthToRuntimeText(String text)
{
  final value = int.parse(normalizeUiDepthValue(text));
  if (value == 0)
  {
    return 'all';
  }
  if (value == 1)
  {
    return 'single';
  }
  return '${value - 1}';
}

List<ModuleDepthRule> parseModuleDepthRules(String text)
{
  final rules = <ModuleDepthRule>[];
  final seen = <String>{};
  for (final raw in text.replaceAll(';', ',').split(','))
  {
    final token = raw.trim();
    if (token.isEmpty)
    {
      continue;
    }
    final parts = token.split(':').map((part) => part.trim()).toList();
    if (parts.length != 2 && parts.length != 3)
    {
      throw FormatException('模块过滤规则格式无效，使用 模块名:层级[:模式]。');
    }
    final moduleName = parts[0];
    if (moduleName.isEmpty)
    {
      throw FormatException('模块过滤规则缺少模块名。');
    }
    final lowered = moduleName.toLowerCase();
    if (seen.contains(lowered))
    {
      throw FormatException('模块过滤规则重复：$moduleName');
    }
    seen.add(lowered);
    rules.add(ModuleDepthRule(
      moduleName: moduleName,
      uiDepth: normalizeUiDepthValue(parts[1]),
      executionMode: normalizeExecutionMode(parts.length == 3 ? parts[2] : 'EDGE'),
    ));
  }
  return rules;
}

String serializeModuleDepthRules(List<ModuleDepthRule> rules)
{
  return rules
      .map((rule) => '${rule.moduleName}:${normalizeUiDepthValue(rule.uiDepth)}:${normalizeExecutionMode(rule.executionMode)}')
      .join(',');
}

String formatUiDepthLabel(String text)
{
  final value = int.parse(normalizeUiDepthValue(text));
  if (value == 0)
  {
    return '不限(ALL)';
  }
  if (value == 1)
  {
    return '同层(SINGLE)';
  }
  return '向下${value - 1}层';
}

String formatRuleLabel(String uiDepth, String executionMode)
{
  return '${formatUiDepthLabel(uiDepth)} / ${normalizeExecutionMode(executionMode)}';
}

String buildDepthFilterSpec(
  bool outsideEnabled,
  String outsideUiDepth,
  String outsideExecutionMode,
  bool anonymousEnabled,
  String anonymousUiDepth,
  String anonymousExecutionMode,
  String moduleRulesText,
)
{
  final tokens = <String>[];
  if (outsideEnabled)
  {
    tokens.add('outside=${uiDepthToRuntimeText(outsideUiDepth)}:${runtimeExecutionModeText(outsideExecutionMode)}');
  }
  if (anonymousEnabled)
  {
    tokens.add('anon=${uiDepthToRuntimeText(anonymousUiDepth)}:${runtimeExecutionModeText(anonymousExecutionMode)}');
  }
  for (final rule in parseModuleDepthRules(moduleRulesText))
  {
    tokens.add('module=${rule.moduleName}:${uiDepthToRuntimeText(rule.uiDepth)}:${runtimeExecutionModeText(rule.executionMode)}');
  }
  return tokens.join(',');
}

String buildDepthFilterSummary(
  String defaultUiDepth,
  bool outsideEnabled,
  String outsideUiDepth,
  String outsideExecutionMode,
  bool anonymousEnabled,
  String anonymousUiDepth,
  String anonymousExecutionMode,
  String moduleRulesText,
  bool idleEscapeEnabled,
  String idleEscapeThreshold,
)
{
  final parts = <String>['默认=${formatUiDepthLabel(defaultUiDepth)}'];
  if (outsideEnabled)
  {
    parts.add('模块外=${formatRuleLabel(outsideUiDepth, outsideExecutionMode)}');
  }
  if (anonymousEnabled)
  {
    parts.add('匿名页=${formatRuleLabel(anonymousUiDepth, anonymousExecutionMode)}');
  }
  final moduleRules = parseModuleDepthRules(moduleRulesText);
  if (moduleRules.isNotEmpty)
  {
    final tfCount = moduleRules.where((rule) => normalizeExecutionMode(rule.executionMode) == 'TF').length;
    parts.add(tfCount == 0 ? '模块规则=${moduleRules.length}条' : '模块规则=${moduleRules.length}条/$tfCount个TF');
  }
  parts.add(idleEscapeEnabled ? '空转跳出=${idleEscapeThreshold.trim().isEmpty ? '32' : idleEscapeThreshold.trim()}' : '空转跳出=关闭');
  return parts.join(' | ');
}
