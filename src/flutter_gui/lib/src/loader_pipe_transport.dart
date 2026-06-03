import 'dart:ffi';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';
import 'package:win32/win32.dart';

import 'models.dart';

const int genericReadWrite = GENERIC_READ | GENERIC_WRITE;
const int messageHeaderSize = 16;
const int maxMessageSize = 1024 * 1024;
const int loadDllReplyPayloadSize = 8 + loaderMaxPathChars * 2 + loaderMaxTextChars * 2;
const int pokeTimeoutMs = 20;

final DynamicLibrary _kernel32 = DynamicLibrary.open('kernel32.dll');
final int Function(Pointer<Utf16> name, int timeoutMs) _waitNamedPipeW =
    _kernel32.lookupFunction<Int32 Function(Pointer<Utf16> name, Uint32 timeoutMs), int Function(Pointer<Utf16> name, int timeoutMs)>('WaitNamedPipeW');

void pokePendingPipe(String pipeName)
{
  final name = pipeName.toNativeUtf16();
  try
  {
    final waited = _waitNamedPipeW(name, pokeTimeoutMs);
    if (waited == 0)
    {
      return;
    }
    final handle = CreateFile(name, genericReadWrite, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle != INVALID_HANDLE_VALUE)
    {
      CloseHandle(handle);
    }
  }
  finally
  {
    calloc.free(name);
  }
}

bool connectPipeBlocking(int pipe)
{
  final connected = ConnectNamedPipe(pipe, nullptr);
  if (connected != 0)
  {
    return true;
  }
  return GetLastError() == ERROR_PIPE_CONNECTED;
}

int? peekPipeAvailable(int pipe)
{
  final available = calloc<DWORD>();
  try
  {
    final ok = PeekNamedPipe(pipe, nullptr, 0, nullptr, available, nullptr);
    if (ok == 0)
    {
      return null;
    }
    return available.value;
  }
  finally
  {
    calloc.free(available);
  }
}

Uint8List? readMessageBlocking(int pipe)
{
  final header = readExactBlocking(pipe, messageHeaderSize);
  if (header == null)
  {
    return null;
  }

  final headerData = ByteData.sublistView(header);
  final magic = headerData.getUint32(0, Endian.little);
  final size = headerData.getUint32(8, Endian.little);
  if (magic != loaderMagic || size < messageHeaderSize || size > maxMessageSize)
  {
    return null;
  }

  final payloadSize = size - messageHeaderSize;
  final payload = payloadSize == 0 ? Uint8List(0) : readExactBlocking(pipe, payloadSize);
  if (payload == null)
  {
    return null;
  }
  return Uint8List.fromList([...header, ...payload]);
}

Uint8List? readExactBlocking(int pipe, int size)
{
  final result = Uint8List(size);
  var offset = 0;
  while (offset < size)
  {
    final chunkSize = size - offset;
    final buffer = calloc<Uint8>(chunkSize);
    final bytesRead = calloc<DWORD>();
    try
    {
      final ok = ReadFile(pipe, buffer, chunkSize, bytesRead, nullptr);
      if (ok == 0 || bytesRead.value == 0)
      {
        return null;
      }
      for (var index = 0; index < bytesRead.value; index++)
      {
        result[offset + index] = buffer[index];
      }
      offset += bytesRead.value;
    }
    finally
    {
      calloc.free(buffer);
      calloc.free(bytesRead);
    }
  }
  return result;
}

bool sendLoadDllRequestBlocking(int handle, String agentPath)
{
  final payloadSize = loaderMaxPathChars * 2;
  final messageSize = messageHeaderSize + payloadSize;
  final message = Uint8List(messageSize);
  final data = ByteData.sublistView(message);
  data.setUint32(0, loaderMagic, Endian.little);
  data.setUint32(4, loaderMsgLoadDllRequest, Endian.little);
  data.setUint32(8, messageSize, Endian.little);
  data.setUint32(12, GetCurrentProcessId(), Endian.little);
  writeUtf16Z(message, messageHeaderSize, loaderMaxPathChars, agentPath);
  return writeAllBlocking(handle, message);
}

bool writeAllBlocking(int pipe, Uint8List bytes)
{
  var offset = 0;
  while (offset < bytes.length)
  {
    final remaining = bytes.length - offset;
    final buffer = calloc<Uint8>(remaining);
    final written = calloc<DWORD>();
    try
    {
      for (var index = 0; index < remaining; index++)
      {
        buffer[index] = bytes[offset + index];
      }
      final ok = WriteFile(pipe, buffer, remaining, written, nullptr);
      if (ok == 0 || written.value == 0)
      {
        return false;
      }
      offset += written.value;
    }
    finally
    {
      calloc.free(buffer);
      calloc.free(written);
    }
  }
  return true;
}

void writeUtf16Z(Uint8List destination, int byteOffset, int maxChars, String value)
{
  final units = value.codeUnits.take(maxChars - 1).toList();
  for (var index = 0; index < units.length; index++)
  {
    final offset = byteOffset + index * 2;
    destination[offset] = units[index] & 0xff;
    destination[offset + 1] = (units[index] >> 8) & 0xff;
  }
}
