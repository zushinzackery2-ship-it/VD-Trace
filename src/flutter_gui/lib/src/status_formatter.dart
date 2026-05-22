String formatTraceStatusText(String message)
{
  if (message.trim().isEmpty)
  {
    return '未拿到追踪状态。';
  }
  final fields = <String, String>{};
  for (final token in message.split(RegExp(r'\s+')))
  {
    if (!token.contains('='))
    {
      continue;
    }
    final index = token.indexOf('=');
    final key = token.substring(0, index);
    final value = token.substring(index + 1);
    if (key.isNotEmpty && value.isNotEmpty)
    {
      fields[key] = value;
    }
  }
  final parts = <String>[fields['running'] == '1' ? '运行中' : '待机'];
  void addIf(String key, String label)
  {
    final value = fields[key];
    if (value != null && value.isNotEmpty)
    {
      parts.add('$label=$value');
    }
  }

  final backend = fields['backend'];
  if (backend != null && backend.isNotEmpty)
  {
    parts.add('后端=${backend.toUpperCase()}');
  }
  if (fields['auto_thread'] == '1')
  {
    parts.add('线程=自动');
  }
  else if (fields['auto_thread'] == '0')
  {
    parts.add('线程=手动');
  }
  if (fields['focus'] == 'queue')
  {
    parts.add('线程模式=轮转');
  }
  else if (fields['focus'] == 'single')
  {
    parts.add('线程模式=单线程');
  }
  if (fields['event_mode'] == 'flow')
  {
    parts.add('记录=控制流');
  }
  else if (fields['event_mode'] == 'full')
  {
    parts.add('记录=全指令');
  }
  final activeThread = fields['active_thread'];
  if (activeThread != null && activeThread != '0')
  {
    parts.add('活动线程=$activeThread');
  }
  addIf('capture', '捕获');
  addIf('capture_hits', '触发命中');
  final captureLast = fields['capture_last'];
  if (captureLast != null && captureLast != '0')
  {
    parts.add('最近命中线程=$captureLast');
  }
  addIf('depth', '当前深度');
  if (fields['scope'] == 'tracked')
  {
    parts.add('范围=仅指定模块');
  }
  else if (fields['scope'] == 'all')
  {
    parts.add('范围=含模块外');
  }
  const observeLabels = <String, String>{
    'idle': '待命',
    'dest': '等目标',
    'tail': '等块尾',
    'single-step': '单步',
    'linear-scan': '线扫',
    'hot-bypass': '空转跳出',
  };
  final observe = observeLabels[fields['observe']];
  if (observe != null)
  {
    parts.add('观测=$observe');
  }
  addIf('steps', '步数');
  addIf('events', '事件');
  addIf('written_events', '已写事件');
  final events = int.tryParse(fields['events'] ?? '');
  final written = int.tryParse(fields['written_events'] ?? '');
  if (events != null && written != null && events - written != 0)
  {
    parts.add('落盘差=${events - written}');
  }
  if (fields['writing'] == '1')
  {
    parts.add('写入=TRUE');
  }
  else if (fields['writing'] == '0')
  {
    parts.add('写入=FALSE');
  }
  for (final item in const [
    ('pending_events', '待写事件'),
    ('pending_write_events', '待写事件(文件)'),
    ('pending_write_bytes', '待写字节'),
    ('dropped_events_total', '已丢事件'),
    ('dropped_write_events', '已丢事件(文件)'),
    ('event_gap', '计数差'),
    ('depth_module_rules', '模块规则'),
    ('hot_streak', '热重复'),
    ('dup_suppressed', '重复边压制'),
    ('outside_suppressed', '范围外静默'),
    ('hot_bypass_count', '空转跳出次数'),
  ])
  {
    final value = fields[item.$1];
    if (value != null && value != '0')
    {
      parts.add('${item.$2}=$value');
    }
  }
  addIf('accounted_events', '已核对');
  addIf('call_limit', '层级上限');
  if (fields['root_stop'] == '1')
  {
    parts.add('停止=根返回');
  }
  else if (fields['root_stop'] == '0')
  {
    parts.add('停止=手动');
  }
  addIf('depth_outside', '模块外层级');
  addIf('depth_anon', '匿名页层级');
  addIf('probes', '观测器');
  addIf('hits', '命中策略');
  addIf('idle_escape', '空转跳出');
  if (fields['scope'] == 'tracked' && (fields['depth_outside'] ?? '').isNotEmpty)
  {
    parts.add('模块外记录=关');
  }
  if (fields['hits'] == 'first' && fields['event_mode'] == 'flow' && fields['root_stop'] == '0')
  {
    parts.add('会话=有限边集');
  }
  addIf('trigger', '触发');
  return parts.join(' | ');
}

bool statusMessageIsWriting(String message)
{
  return RegExp(r'(^|\s)writing=1(\s|$)').hasMatch(message);
}

bool statusMessageIsRunning(String message)
{
  return RegExp(r'(^|\s)running=1(\s|$)').hasMatch(message);
}
