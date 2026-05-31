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
    expect(find.text('自动发现目标'), findsOneWidget);
    expect(find.text('Loader 会话'), findsNothing);
    expect(find.text('Core Trace Setup'), findsOneWidget);
    expect(find.byKey(const ValueKey('top_module_picker')), findsOneWidget);

    final autoTargetPanel = find.byWidgetPredicate((widget)
    {
      final constraints = widget is Container ? widget.constraints : null;
      return widget is Container &&
          constraints != null &&
          constraints.minHeight == 58 &&
          constraints.maxHeight == 58 &&
          widget.padding == const EdgeInsets.symmetric(horizontal: 12, vertical: 8);
    });
    expect(autoTargetPanel, findsOneWidget);
    expect(tester.getTopLeft(autoTargetPanel).dx, tester.getTopLeft(find.byKey(const ValueKey('vdtrace_brand_title'))).dx);
  });
}
