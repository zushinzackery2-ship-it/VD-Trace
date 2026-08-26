import 'package:flutter_test/flutter_test.dart';

import 'package:vdtrace_gui/src/vdtrace_app.dart';

void main()
{
  testWidgets('navigation rail switches the active configuration section', (WidgetTester tester) async
  {
    await tester.pumpWidget(const VdTraceFlutterApp(startRuntime: false));

    // The core section renders first.
    expect(find.text('Core Trace Setup'), findsOneWidget);

    // Switching to the policy section reveals the Run Policy card.
    await tester.tap(find.text('策略'));
    await tester.pumpAndSettle();
    expect(find.text('Run Policy'), findsOneWidget);

    // And to the memory tool.
    await tester.tap(find.text('内存'));
    await tester.pumpAndSettle();
    expect(find.text('Memory Tool'), findsOneWidget);
  });
}
