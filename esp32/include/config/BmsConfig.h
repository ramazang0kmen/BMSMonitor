#pragma once

#include <Arduino.h>

namespace BmsConfig {
struct Definition { const char* name; const char* mac; };
constexpr Definition DEVICES[2] = {
    {"PPD26C0259", "a5:c2:3a:20:e5:1f"},
    {"PPD25L0232", "a5:c2:37:24:6b:41"},
};
constexpr uint32_t POLL_INTERVAL_MS = 1500;
constexpr uint32_t RESPONSE_TIMEOUT_MS = 1500;
constexpr uint32_t DATA_STALE_MS = 5000;
constexpr uint32_t CONNECT_TIMEOUT_MS = 8000;
constexpr uint32_t SUBSCRIBE_SETTLE_MS = 1200;
constexpr uint32_t RECONNECT_BACKOFF_MS[] = {2000, 5000, 10000};
constexpr bool DEBUG_JBD_RAW = false;
}  // namespace BmsConfig
