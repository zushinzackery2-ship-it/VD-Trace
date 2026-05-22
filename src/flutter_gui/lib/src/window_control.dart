import 'dart:ffi';

import 'package:ffi/ffi.dart';
import 'package:win32/win32.dart';

const int wmVdTraceMinimize = WM_APP + 0x501;
const int wmVdTraceMaximizeRestore = WM_APP + 0x502;
const int wmVdTraceClose = WM_APP + 0x503;

void minimizeWindow()
{
  _postCommand(wmVdTraceMinimize);
}

void maximizeOrRestoreWindow()
{
  _postCommand(wmVdTraceMaximizeRestore);
}

void closeWindow()
{
  _postCommand(wmVdTraceClose);
}

void startWindowDrag()
{
  final hwnd = _activeWindowHandle();
  if (hwnd == 0)
  {
    return;
  }
  ReleaseCapture();
  SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}

void _postCommand(int message)
{
  final hwnd = _activeWindowHandle();
  if (hwnd != 0)
  {
    PostMessage(hwnd, message, 0, 0);
    return;
  }
}

int _activeWindowHandle()
{
  final hwnd = GetActiveWindow();
  if (hwnd != 0)
  {
    final root = GetAncestor(hwnd, GA_ROOT);
    return root == 0 ? hwnd : root;
  }
  final className = 'FLUTTER_RUNNER_WIN32_WINDOW'.toNativeUtf16();
  try
  {
    return FindWindow(className, nullptr);
  }
  finally
  {
    calloc.free(className);
  }
}
