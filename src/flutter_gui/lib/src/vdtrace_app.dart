import 'package:flutter/material.dart';

import 'shell/home_page.dart';
import 'theme/app_theme.dart';

/// Top-level application widget wiring the theme to the control shell.
class VdTraceFlutterApp extends StatelessWidget
{
  const VdTraceFlutterApp({super.key, this.startRuntime = true});

  /// When false the runtime poll loop and Loader bridge are not started, which
  /// keeps widget tests deterministic and side-effect free.
  final bool startRuntime;

  @override
  Widget build(BuildContext context)
  {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: 'VD-Trace',
      theme: buildVdTraceTheme(),
      home: VdTraceHomePage(startRuntime: startRuntime),
    );
  }
}
