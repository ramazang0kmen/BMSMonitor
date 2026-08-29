import 'package:bmsmonitor/core/constants/app_constants.dart';
import 'package:bmsmonitor/core/utils/telemetry_state.dart';
import 'package:bmsmonitor/features/dashboard/domain/models/gateway_status.dart';
import 'package:bmsmonitor/features/dashboard/domain/models/live_telemetry.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  final now = DateTime.fromMillisecondsSinceEpoch(2_000_000);

  GatewayStatus statusAt(Duration age) => GatewayStatus.fromJson({
    'online': false,
    'lastSeen': now.millisecondsSinceEpoch - age.inMilliseconds,
  });

  LiveTelemetry liveAt(Duration age) => LiveTelemetry.fromJson({
    'timestamp': now.millisecondsSinceEpoch - age.inMilliseconds,
  });

  test('gateway online is based on lastSeen rather than online flag', () {
    expect(isGatewayOnline(statusAt(const Duration(seconds: 59)), now), isTrue);
    expect(
      isGatewayOnline(statusAt(AppConstants.gatewayOnlineTimeout), now),
      isTrue,
    );
    expect(
      isGatewayOnline(statusAt(const Duration(seconds: 61)), now),
      isFalse,
    );
    expect(isGatewayOnline(null, now), isFalse);
  });

  test('classifies live data freshness at boundaries', () {
    final status = statusAt(Duration.zero);
    expect(
      liveFreshness(status, liveAt(Duration.zero), now),
      LiveFreshness.live,
    );
    expect(
      liveFreshness(status, liveAt(const Duration(seconds: 5)), now),
      LiveFreshness.live,
    );
    expect(
      liveFreshness(status, liveAt(const Duration(seconds: 10)), now),
      LiveFreshness.delayed,
    );
    expect(
      liveFreshness(status, liveAt(const Duration(seconds: 16)), now),
      LiveFreshness.stale,
    );
  });

  test('gateway offline overrides telemetry and missing live waits', () {
    expect(
      liveFreshness(
        statusAt(const Duration(seconds: 61)),
        liveAt(Duration.zero),
        now,
      ),
      LiveFreshness.offline,
    );
    expect(
      liveFreshness(statusAt(Duration.zero), null, now),
      LiveFreshness.waiting,
    );
  });
}
