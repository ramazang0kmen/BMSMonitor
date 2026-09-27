#pragma once

#include <Arduino.h>

// Thresholds for the Firestore event log (devices/{id}/logs). Every "enter"
// threshold has a separate, looser "re-arm" threshold so a value hovering
// around the limit logs once instead of flapping, and every condition must
// hold for a while before it is logged so a single glitchy BLE frame cannot
// produce an entry.
namespace LogConfig {
constexpr float SOC_LOW_PCT = 20.0f;
constexpr float SOC_LOW_REARM_PCT = 25.0f;
// The averaged SOC is derived from remaining/total Ah, so it rarely lands on
// exactly 0.0; anything that would display as 0 % counts as empty.
constexpr float SOC_EMPTY_PCT = 0.5f;
constexpr float SOC_EMPTY_REARM_PCT = 5.0f;
constexpr float SOC_FULL_PCT = 99.5f;
constexpr float SOC_FULL_REARM_PCT = 95.0f;
constexpr uint32_t SOC_HOLD_MS = 5000;

// JBD reports discharge as negative current. Flip this if the BMS on site
// turns out to use the opposite convention.
constexpr float DISCHARGE_CURRENT_SIGN = -1.0f;
// "Load is on" means at least this much is being drawn from the batteries.
// "Load dropped to zero" means the draw fell below the idle limit; an inverter
// that is merely idling still pulls more than this, so this marks a real cut.
constexpr float LOAD_ACTIVE_A = 1.0f;
constexpr float LOAD_IDLE_A = 0.3f;
constexpr uint32_t LOAD_HOLD_MS = 10000;

constexpr uint32_t MOS_HOLD_MS = 3000;
constexpr uint32_t PROTECTION_HOLD_MS = 2000;
constexpr uint32_t BMS_OFFLINE_HOLD_MS = 60000;
constexpr float SOC_MISMATCH_REARM_PCT = 7.0f;
constexpr uint32_t SOC_MISMATCH_HOLD_MS = 30000;

// The same condition cannot log again sooner than this, whatever it does.
constexpr uint32_t EVENT_COOLDOWN_MS = 5UL * 60UL * 1000UL;

constexpr size_t QUEUE_CAPACITY = 16;
constexpr uint8_t MAX_WRITE_ATTEMPTS = 5;
constexpr uint32_t WRITE_RETRY_BACKOFF_MS[] = {5000, 15000, 30000, 60000};
constexpr char COLLECTION[] = "logs";
}  // namespace LogConfig
