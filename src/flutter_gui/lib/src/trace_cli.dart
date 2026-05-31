import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'models.dart';

class TraceCli
{
  TraceCli({required this.ctlPath, required this.workdir});

  final String ctlPath;
  final Directory workdir;

  Future<CommandResult> run(List<String> args) async
  {
    if (!File(ctlPath).existsSync())
    {
      return CommandResult(success: false, message: '找不到控制端：$ctlPath');
    }
    final completed = await Process.run(
      ctlPath,
      args,
      workingDirectory: workdir.path,
      stdoutEncoding: utf8,
      stderrEncoding: utf8,
    );
    final stdoutText = (completed.stdout as String?)?.trim() ?? '';
    final stderrText = (completed.stderr as String?)?.trim() ?? '';
    final output = stdoutText.isNotEmpty ? stdoutText : stderrText;
    final success = output.startsWith('[ok] ');
    final message = success ? output.substring(5) : output.startsWith('[fail] ') ? output.substring(7) : output;
    return CommandResult(success: success, message: message.isEmpty ? '命令没有输出。' : message, rawOutput: output);
  }

  Future<CommandResult> ping(int pid) => run(['ping', '$pid']);

  Future<CommandResult> status(int pid) => run(['status', '$pid']);

  Future<CommandResult> start(int pid) => run(['start', '$pid']);

  Future<CommandResult> stop(int pid) => run(['stop', '$pid']);

  Future<CommandResult> modules(int pid, {bool includeSystemModules = false})
  {
    final args = ['modules', '$pid'];
    if (includeSystemModules)
    {
      args.add('all');
    }
    return run(args);
  }

  Future<CommandResult> dumpModule(int pid, String moduleName, {String outputDirectory = r'.\dump'})
  {
    return run(['dump', '$pid', moduleName, outputDirectory]);
  }

  Future<CommandResult> readMemory(int pid, String addressText, int size)
  {
    return run(['read', '$pid', addressText, '$size']);
  }

  Future<CommandResult> writeMemory(int pid, String addressText, Uint8List data)
  {
    return run(['write', '$pid', addressText, _hex(data)]);
  }

  Future<CommandResult> inject(int pid, String agentPath)
  {
    return run(['inject', '$pid', agentPath]);
  }

  Future<CommandResult> configure(int pid, TraceConfig config)
  {
    return run(config.cliArgs(pid));
  }

  Future<bool> waitUntilOnline(int pid, int timeoutMs) async
  {
    final deadline = DateTime.now().add(Duration(milliseconds: timeoutMs));
    while (DateTime.now().isBefore(deadline))
    {
      if ((await ping(pid)).success)
      {
        return true;
      }
      await Future<void>.delayed(const Duration(milliseconds: 100));
    }
    return (await ping(pid)).success;
  }

  String _hex(Uint8List data)
  {
    final buffer = StringBuffer();
    for (final byte in data)
    {
      buffer.write(byte.toRadixString(16).padLeft(2, '0'));
    }
    return buffer.toString();
  }
}
