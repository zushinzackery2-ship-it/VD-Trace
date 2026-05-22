import 'package:flutter_test/flutter_test.dart';

import 'package:vdtrace_gui/src/vdtrace_app.dart';

void main()
{
  testWidgets('VD-Trace shell renders', (WidgetTester tester) async
  {
    await tester.pumpWidget(const VdTraceFlutterApp(startRuntime: false));

    expect(find.text('VD-Trace'), findsOneWidget);
    expect(find.text('Core Trace Setup'), findsOneWidget);
  });
}
