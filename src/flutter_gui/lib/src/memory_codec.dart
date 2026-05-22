import 'dart:convert';
import 'dart:typed_data';

String normalizeMemoryWriteMode(String text)
{
  final lowered = text.trim().toLowerCase();
  if (lowered == 'text')
  {
    return 'TEXT';
  }
  if (lowered == 'utf16')
  {
    return 'UTF16';
  }
  if (lowered == 'u32')
  {
    return 'U32';
  }
  if (lowered == 'u64')
  {
    return 'U64';
  }
  return 'HEX';
}

int parseMemorySize(String text)
{
  final size = int.parse(text.trim().isEmpty ? '64' : text.trim());
  if (size <= 0 || size > 512)
  {
    throw FormatException('读取长度必须是 1 到 512 之间的整数。');
  }
  return size;
}

Uint8List encodeMemoryWriteBytes(String mode, String text)
{
  final normalized = normalizeMemoryWriteMode(mode);
  final raw = text.trim();
  late final List<int> data;
  if (normalized == 'HEX')
  {
    final compact = raw.replaceAll(RegExp(r'[\s,\-]'), '');
    if (compact.isEmpty || compact.length.isOdd || !RegExp(r'^[0-9a-fA-F]+$').hasMatch(compact))
    {
      throw FormatException('HEX 写入需要偶数个十六进制字符。');
    }
    data = <int>[];
    for (var i = 0; i < compact.length; i += 2)
    {
      data.add(int.parse(compact.substring(i, i + 2), radix: 16));
    }
  }
  else if (normalized == 'TEXT')
  {
    data = utf8.encode(raw);
  }
  else if (normalized == 'UTF16')
  {
    data = <int>[];
    for (final codeUnit in raw.codeUnits)
    {
      data.add(codeUnit & 0xff);
      data.add((codeUnit >> 8) & 0xff);
    }
  }
  else if (normalized == 'U32')
  {
    final value = int.parse(raw);
    if (value < 0 || value > 0xffffffff)
    {
      throw FormatException('U32 写入值必须在 0 到 0xFFFFFFFF 之间。');
    }
    final bytes = ByteData(4)..setUint32(0, value, Endian.little);
    return _checkedBytes(bytes.buffer.asUint8List());
  }
  else
  {
    final value = BigInt.parse(raw);
    if (value < BigInt.zero || value > BigInt.parse('ffffffffffffffff', radix: 16))
    {
      throw FormatException('U64 写入值必须在 0 到 0xFFFFFFFFFFFFFFFF 之间。');
    }
    final bytes = Uint8List(8);
    var shifted = value;
    for (var i = 0; i < 8; i++)
    {
      bytes[i] = (shifted & BigInt.from(0xff)).toInt();
      shifted = shifted >> 8;
    }
    return _checkedBytes(bytes);
  }
  return _checkedBytes(Uint8List.fromList(data));
}

Uint8List _checkedBytes(Uint8List data)
{
  if (data.isEmpty)
  {
    throw FormatException('写入内容不能为空。');
  }
  if (data.length > 512)
  {
    throw FormatException('写入内容不能超过 512 字节。');
  }
  return data;
}
