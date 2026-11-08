#pragma once

#include <Arduino.h>

namespace BmsConfig {
struct Definition { const char* name; const char* mac; };

// TEST ONLY: publishes nine synthetic packs without requiring nearby BLE BMS
// hardware. Set to false before returning to real batteries.
constexpr bool TEST_BMS_MODE = false;
constexpr size_t TEST_BMS_COUNT = 9;
constexpr uint32_t TEST_UPDATE_INTERVAL_MS = 1500;
// Used only when a boot scan finds no JBD advertisement at all. Normal device
// discovery is dynamic and does not depend on this list.
constexpr Definition FALLBACK_DEVICES[] = {
    {"PPD26C0259", "a5:c2:3a:20:e5:1f"},
    {"PPD25L0232", "a5:c2:37:24:6b:41"},
};
constexpr size_t MAX_TRACKED_BMS = 12;
constexpr size_t MAX_ACTIVE_CONNECTIONS = 3;
constexpr uint32_t SCAN_DURATION_MS = 10000;
constexpr uint32_t POLL_INTERVAL_MS = 1500;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 1500;
constexpr uint32_t DATA_STALE_MS = 5000;
constexpr uint32_t ROTATING_DATA_STALE_MS = 90000;
constexpr uint32_t CONNECT_TIMEOUT_MS = 8000;
constexpr uint32_t SUBSCRIBE_SETTLE_MS = 1200;
constexpr uint32_t GROUP_MIN_DWELL_MS = 3000;
constexpr uint32_t GROUP_MAX_DWELL_MS = 18000;
constexpr uint32_t GROUP_SWITCH_DELAY_MS = 500;
constexpr uint32_t RECONNECT_BACKOFF_MS[] = {2000, 5000, 10000};
constexpr bool DEBUG_JBD_RAW = false;
}  // namespace BmsConfig
