import 'dart:async';

typedef HeartbeatWriter = Future<void> Function();

class ViewerHeartbeatController {
  ViewerHeartbeatController({
    required HeartbeatWriter writeHeartbeat,
    this.interval = const Duration(seconds: 5),
    this.onError,
  }) : _writeHeartbeat = writeHeartbeat;

  final HeartbeatWriter _writeHeartbeat;
  final Duration interval;
  final void Function(Object error)? onError;
  Timer? _timer;

  bool get isRunning => _timer?.isActive ?? false;

  void start() {
    if (isRunning) return;
    _send();
    _timer = Timer.periodic(interval, (_) => _send());
  }

  void stop() {
    _timer?.cancel();
    _timer = null;
  }

  Future<void> _send() async {
    try {
      await _writeHeartbeat();
    } catch (error) {
      onError?.call(error);
    }
  }

  void dispose() => stop();
}
