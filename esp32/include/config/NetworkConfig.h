#pragma once

#include <Arduino.h>

namespace NetworkConfig {
constexpr char FIRMWARE_VERSION[] = "0.4.1-stability";
constexpr uint32_t STATUS_HEARTBEAT_INTERVAL_MS = 30000;
constexpr uint32_t REMOTE_SESSION_CHECK_INTERVAL_MS = 2000;
constexpr uint32_t REMOTE_SESSION_DURATION_MS = 60000;
constexpr uint32_t TELEMETRY_INTERVAL_MS = 1000;
constexpr uint32_t HEAP_LOG_INTERVAL_MS = 60000;
constexpr uint32_t FIREBASE_HEALTH_LOG_INTERVAL_MS = 30000;
constexpr uint32_t FIREBASE_OPERATION_TIMEOUT_MS = 10000;
constexpr uint32_t FIREBASE_CONTROL_STALE_WARN_MS = 15000;
constexpr uint32_t FIREBASE_RECOVERY_THRESHOLD_MS = 30000;
constexpr uint32_t FIREBASE_RECOVERY_BACKOFF_MS[] = {2000, 5000, 10000, 30000};
constexpr uint32_t WIFI_RECONNECT_BACKOFF_MS[] = {2000, 5000, 10000, 30000};
constexpr bool DEBUG_NETWORK = true;
constexpr bool DEBUG_FIREBASE = false;
#ifndef DEBUG_FIREBASE_HEALTH
#define DEBUG_FIREBASE_HEALTH 0
#endif
constexpr bool DEBUG_FIREBASE_HEALTH_LOGS = DEBUG_FIREBASE_HEALTH != 0;
}  // namespace NetworkConfig
