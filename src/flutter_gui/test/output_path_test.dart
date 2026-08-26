import 'package:flutter_test/flutter_test.dart';
import 'package:vdtrace_gui/src/models.dart';

void main()
{
  test('defaultOutputPath interpolates the pid instead of emitting a literal token', ()
  {
    final path = defaultOutputPath(4321);
    expect(path, contains('VDTrace-4321-'));
    expect(path, isNot(contains(r'$pid')));
    expect(path, endsWith('.log'));
  });

  test('an auto-generated output path is recognized as auto for its pid', ()
  {
    const pid = 4321;
    expect(isAutoOutputPath(defaultOutputPath(pid), pid), isTrue);
  });

  test('a user-chosen output path is not treated as auto', ()
  {
    expect(isAutoOutputPath(r'C:\traces\my-run.log', 4321), isFalse);
    expect(isAutoOutputPath('', 4321), isTrue);
  });
}
