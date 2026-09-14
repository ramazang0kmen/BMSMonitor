#pragma once

#include <Arduino.h>

namespace NetworkConfig {
constexpr char FIRMWARE_VERSION[] = "0.4.6-stability";
constexpr uint32_t STATUS_HEARTBEAT_INTERVAL_MS = 30000;
constexpr uint32_t REMOTE_SESSION_CHECK_INTERVAL_MS = 2000;
constexpr uint32_t REMOTE_SESSION_DURATION_MS = 60000;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 1000;
constexpr uint32_t HEAP_LOG_INTERVAL_MS = 60000;
constexpr uint32_t FIREBASE_HEALTH_LOG_INTERVAL_MS = 30000;
constexpr uint32_t FIREBASE_OPERATION_TIMEOUT_MS = 10000;
// Upper bound on how long we wait for FirebaseClient's internal async queue
// (asyncClient.taskCount()) to drain after asyncClient.stopAsync(true) flags a
// task for removal. The library clears the flagged slot on its next app.loop()
// pass; past this deadline we stop gating new traffic on it and let the
// library re-establish its own transport on the next request. We deliberately
// do NOT reach past the library to close its socket - doing so corrupts
// FirebaseClient's connection state machine and permanently wedges all RTDB
// traffic (observed on hardware: every operation afterwards hangs with no
// callback and no error).
constexpr uint32_t FIREBASE_QUEUE_DRAIN_TIMEOUT_MS = 4000;
constexpr uint32_t FIREBASE_CONTROL_STALE_WARN_MS = 15000;
constexpr uint32_t FIREBASE_RECOVERY_THRESHOLD_MS = 30000;
// Per-channel "this channel has stopped making progress" thresholds. Each MUST
// be comfortably larger than that channel's own send interval, or ordinary
// round-trip jitter on a single cycle trips a false recovery. Control reads go
// out every REMOTE_SESSION_CHECK_INTERVAL_MS (2 s); the status heartbeat only
// every STATUS_HEARTBEAT_INTERVAL_MS (30 s), so its threshold must tolerate a
// few missed beats - previously it was equal to the interval and fired on a
// normal slightly-late heartbeat. Live telemetry runs ~1 s while a viewer is
// active.
constexpr uint32_t FIREBASE_CONTROL_STALE_MS = 30000;
constexpr uint32_t FIREBASE_STATUS_STALE_MS = 3 * STATUS_HEARTBEAT_INTERVAL_MS + 15000;  // 105 s
constexpr uint32_t FIREBASE_LIVE_STALE_MS = 30000;
constexpr uint32_t FIREBASE_RECOVERY_BACKOFF_MS[] = {2000, 5000, 10000, 30000};
constexpr uint32_t WIFI_RECONNECT_BACKOFF_MS[] = {2000, 5000, 10000, 30000};
constexpr bool DEBUG_NETWORK = true;
constexpr bool DEBUG_FIREBASE = false;
#ifndef DEBUG_FIREBASE_HEALTH
#define DEBUG_FIREBASE_HEALTH 0
#endif
constexpr bool DEBUG_FIREBASE_HEALTH_LOGS = DEBUG_FIREBASE_HEALTH != 0;
}  // namespace NetworkConfig
