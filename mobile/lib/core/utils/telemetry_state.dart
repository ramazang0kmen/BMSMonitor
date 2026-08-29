import '../constants/app_constants.dart';
import '../../features/dashboard/domain/models/gateway_status.dart';
import '../../features/dashboard/domain/models/live_telemetry.dart';

enum LiveFreshness { live, delayed, stale, offline, waiting }

bool isGatewayOnline(GatewayStatus? status, DateTime now) {
  if (status == null || status.lastSeen <= 0) return false;
  final age = now.millisecondsSinceEpoch - status.lastSeen;
  return age >= -5000 &&
      age <= AppConstants.gatewayOnlineTimeout.inMilliseconds;
}

LiveFreshness liveFreshness(
  GatewayStatus? status,
  LiveTelemetry? telemetry,
  DateTime now,
) {
  if (!isGatewayOnline(status, now)) return LiveFreshness.offline;
  if (telemetry == null || telemetry.timestamp <= 0) {
    return LiveFreshness.waiting;
  }
  final age = (now.millisecondsSinceEpoch - telemetry.timestamp).clamp(
    0,
    1 << 62,
  );
  if (age <= AppConstants.liveThreshold.inMilliseconds) {
    return LiveFreshness.live;
  }
  if (age <= AppConstants.delayedThreshold.inMilliseconds) {
    return LiveFreshness.delayed;
  }
  return LiveFreshness.stale;
}

String freshnessLabel(LiveFreshness freshness) => switch (freshness) {
  LiveFreshness.live => 'CANLI',
  LiveFreshness.delayed => 'GECİKMELİ',
  LiveFreshness.stale => 'VERİ ESKİ',
  LiveFreshness.offline => 'OFFLINE',
  LiveFreshness.waiting => 'BEKLENİYOR',
};
