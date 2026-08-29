import 'package:bmsmonitor/core/services/viewer_heartbeat_controller.dart';
import 'package:fake_async/fake_async.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('sends immediately, repeats, prevents duplicates, and stops', () {
    fakeAsync((async) {
      var writes = 0;
      final controller = ViewerHeartbeatController(
        writeHeartbeat: () async => writes++,
      );

      controller.start();
      controller.start();
      expect(writes, 1);
      expect(controller.isRunning, isTrue);

      async.elapse(const Duration(seconds: 15));
      expect(writes, 4);

      controller.stop();
      async.elapse(const Duration(seconds: 10));
      expect(writes, 4);
      expect(controller.isRunning, isFalse);
    });
  });

  test('reports writer failures without stopping the timer', () {
    fakeAsync((async) {
      var errors = 0;
      final controller = ViewerHeartbeatController(
        writeHeartbeat: () => Future<void>.error(StateError('write failed')),
        onError: (_) => errors++,
      );

      controller.start();
      async.flushMicrotasks();
      expect(errors, 1);

      async.elapse(const Duration(seconds: 5));
      async.flushMicrotasks();
      expect(errors, 2);
      controller.dispose();
    });
  });
}
