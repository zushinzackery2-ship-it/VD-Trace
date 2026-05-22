import 'dart:convert';
import 'dart:io';

class TracePreviewResult
{
  const TracePreviewResult({required this.status, required this.text});

  final String status;
  final String text;
}

class TracePreviewBuffer
{
  static const int _tailLineLimit = 4096;
  static const int _tailReadChunk = 65536;

  String _tracePath = '';
  String _traceSignature = '-1:-1';
  int _traceLineCount = 0;
  bool _awaitingNewData = false;
  String _text = '';

  TracePreviewResult beginRound(String agentPath, String outputPath)
  {
    final tracePath = _resolvedPath(agentPath, outputPath);
    final signature = tracePath.isEmpty ? '-1:-1' : _pathSignature(File(tracePath));
    _reset(tracePath, signature, true);
    return const TracePreviewResult(status: '已清空预览，等待新追踪。', text: '');
  }

  TracePreviewResult refresh(String agentPath, String outputPath, String triggerPoint)
  {
    final tracePath = _resolvedPath(agentPath, outputPath);
    if (tracePath != _tracePath)
    {
      _reset(tracePath, '-1:-1', true);
    }
    if (tracePath.isEmpty)
    {
      return TracePreviewResult(status: '还没填写输出路径。', text: _text);
    }
    final file = File(tracePath);
    if (!file.existsSync())
    {
      return TracePreviewResult(status: '等待输出文件生成: ${_basename(file.path)}', text: _text);
    }
    final signature = _pathSignature(file);
    if (signature == _traceSignature)
    {
      return TracePreviewResult(status: _statusText(_basename(file.path), triggerPoint), text: _text);
    }
    final tail = _readTailText(file);
    _tracePath = tracePath;
    _traceSignature = signature;
    _traceLineCount = tail.$2;
    _awaitingNewData = tail.$2 == 0;
    _text = tail.$1;
    return TracePreviewResult(status: _statusText(_basename(file.path), triggerPoint), text: _text);
  }

  void _reset(String tracePath, String signature, bool awaitingNewData)
  {
    _tracePath = tracePath;
    _traceSignature = signature;
    _traceLineCount = 0;
    _awaitingNewData = awaitingNewData;
    _text = '';
  }

  String _statusText(String fileName, String triggerPoint)
  {
    if (_traceLineCount == 0 && _awaitingNewData)
    {
      return triggerPoint.isNotEmpty ? '输出文件已创建，正在等待命中触发点：$triggerPoint' : '输出文件已创建，正在等待第一条追踪事件：$fileName';
    }
    return '当前显示日志末端 $_traceLineCount 行，输出文件 $fileName。';
  }

  (String, int) _readTailText(File file)
  {
    final tail = _readTailBytes(file);
    if (tail.isEmpty)
    {
      return ('', 0);
    }
    final text = utf8.decode(tail, allowMalformed: true);
    var lines = const LineSplitter().convert(text);
    if (lines.length > _tailLineLimit)
    {
      lines = lines.sublist(lines.length - _tailLineLimit);
    }
    return (lines.join('\n'), lines.length);
  }

  List<int> _readTailBytes(File file)
  {
    final raf = file.openSync();
    try
    {
      var position = raf.lengthSync();
      final chunks = <List<int>>[];
      var newlineCount = 0;
      while (position > 0 && newlineCount <= _tailLineLimit)
      {
        final readSize = position < _tailReadChunk ? position : _tailReadChunk;
        position -= readSize;
        raf.setPositionSync(position);
        final chunk = raf.readSync(readSize);
        if (chunk.isEmpty)
        {
          break;
        }
        chunks.add(chunk);
        newlineCount += chunk.where((byte) => byte == 10).length;
      }
      return chunks.reversed.expand((chunk) => chunk).toList(growable: false);
    }
    finally
    {
      raf.closeSync();
    }
  }

  String _pathSignature(File file)
  {
    if (!file.existsSync())
    {
      return '-1:-1';
    }
    final stat = file.statSync();
    return '${stat.size}:${stat.modified.microsecondsSinceEpoch}';
  }

  String _resolvedPath(String agentPath, String outputPath)
  {
    final output = outputPath.trim();
    if (output.isEmpty)
    {
      return '';
    }
    final outputFile = File(output);
    if (outputFile.isAbsolute)
    {
      return outputFile.absolute.path;
    }
    return File('${File(agentPath).absolute.parent.path}${Platform.pathSeparator}$output').absolute.path;
  }

  String _basename(String path)
  {
    return path.split(RegExp(r'[\\/]')).last;
  }
}
