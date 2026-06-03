import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:vdtrace_gui/src/vdtrace_app.dart';

void main()
{
  testWidgets('VD-Trace shell renders', (WidgetTester tester) async
  {
    await tester.pumpWidget(const VdTraceFlutterApp(startRuntime: false));

    expect(find.text('VD-Trace'), findsOneWidget);
    expect(find.text('一键加载 Agent'), findsWidgets);
    expect(find.text('Core Trace Setup'), findsOneWidget);
    expect(find.byKey(const ValueKey('vdtrace_brand_title')), findsOneWidget);
    expect(find.byKey(const ValueKey('top_module_picker')), findsOneWidget);
  });
}
