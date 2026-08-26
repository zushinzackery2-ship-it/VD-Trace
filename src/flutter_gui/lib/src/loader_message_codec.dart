import 'dart:typed_data';

import 'loader_pipe_transport.dart';
import 'models.dart';

/// A decoded Loader control frame header plus its payload slice.
class LoaderFrame
{
  const LoaderFrame({required this.kind, required this.pid, required this.payload});

  final int kind;
  final int pid;
  final Uint8List payload;
}

/// Agent handshake metadata decoded from an [loaderMsgAgentHello] payload.
class AgentHello
{
  const AgentHello({required this.processPath, required this.protocolVersion, required this.featureFlags});

  final String processPath;
  final int protocolVersion;
  final int featureFlags;
}

/// Result of a target-side `LoadLibrary` attempt.
class LoadDllReply
{
  const LoadDllReply({required this.status, required this.win32Error, required this.path, required this.text});

  final int status;
  final int win32Error;
  final String path;
  final String text;

  bool get ok => status == 0;
}

/// Splits a raw pipe message into its header fields and payload slice.
LoaderFrame parseLoaderFrame(Uint8List message)
{
  final data = ByteData.sublistView(message);
  final kind = data.getUint32(4, Endian.little);
  final size = data.getUint32(8, Endian.little);
  final pid = data.getUint32(12, Endian.little);
  final payload = message.sublist(messageHeaderSize, size);
  return LoaderFrame(kind: kind, pid: pid, payload: payload);
}

AgentHello parseAgentHello(Uint8List payload)
{
  final processPath = readUtf16Z(payload, 0, loaderMaxPathChars);
  final metadataOffset = loaderMaxPathChars * 2;
  final data = ByteData.sublistView(payload);
  final protocolVersion = payload.length >= metadataOffset + 4 ? data.getUint32(metadataOffset, Endian.little) : 0;
  final featureFlags = payload.length >= metadataOffset + 8 ? data.getUint32(metadataOffset + 4, Endian.little) : 0;
  return AgentHello(processPath: processPath, protocolVersion: protocolVersion, featureFlags: featureFlags);
}

/// Returns null when the payload is too short to contain a complete reply.
LoadDllReply? parseLoadDllReply(Uint8List payload)
{
  if (payload.length < loadDllReplyPayloadSize)
  {
    return null;
  }
  final data = ByteData.sublistView(payload);
  return LoadDllReply(
    status: data.getUint32(0, Endian.little),
    win32Error: data.getUint32(4, Endian.little),
    path: readUtf16Z(payload, 8, loaderMaxPathChars),
    text: readUtf16Z(payload, 8 + loaderMaxPathChars * 2, loaderMaxTextChars),
  );
}

/// Reads a NUL-terminated little-endian UTF-16 string from [payload].
String readUtf16Z(Uint8List payload, int byteOffset, int maxChars)
{
  final units = <int>[];
  for (var index = 0; index < maxChars; index++)
  {
    final offset = byteOffset + index * 2;
    if (offset + 1 >= payload.length)
    {
      break;
    }
    final unit = payload[offset] | (payload[offset + 1] << 8);
    if (unit == 0)
    {
      break;
    }
    units.add(unit);
  }
  return String.fromCharCodes(units);
}

/// Reads a NUL-terminated single-byte string from [payload].
String readAnsiZ(Uint8List payload, int byteOffset, int maxChars)
{
  final bytes = <int>[];
  for (var index = 0; index < maxChars; index++)
  {
    final offset = byteOffset + index;
    if (offset >= payload.length || payload[offset] == 0)
    {
      break;
    }
    bytes.add(payload[offset]);
  }
  return String.fromCharCodes(bytes);
}
