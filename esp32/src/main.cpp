#include <Arduino.h>
#include <NimBLEDevice.h>

#include "ble/BmsManager.h"
#include "config/NetworkConfig.h"
#include "display/DisplayManager.h"
#include "models/NetworkState.h"
#include "network/FirebaseService.h"
#include "network/TimeService.h"
#include "network/WiFiManager.h"

namespace {
BmsManager bmsManager;
DisplayManager displayManager;
GatewayWiFiManager wifiManager;
TimeService timeService;
FirebaseService firebaseService;
NetworkState networkState;
uint32_t nextLogMs = 0;
uint32_t nextHeapLogMs = 0;

void printOperatingData(uint32_t now) {
  if (static_cast<int32_t>(now - nextLogMs) < 0) return;
  nextLogMs = now + 2000U;
  for (size_t i = 0; i < 2; ++i) {
    const BmsDeviceState& state = bmsManager.device(i);
    if (!state.connected || state.lastUpdateMs == 0) {
      Serial.printf("[BMS%u] OFFLINE | errors %lu\n", static_cast<unsigned>(i + 1),
                    static_cast<unsigned long>(state.errorCount));
    } else {
      Serial.printf("[BMS%u] %.2fV | %.2fA | %u%% | %.2fAh | Delta %.0fmV | RSSI %d | dt %lums\n",
                    static_cast<unsigned>(i + 1), state.data.voltage, state.data.current,
                    state.data.soc, state.data.remainingCapacityAh,
                    state.data.cellDelta * 1000.0f, state.rssi,
                    static_cast<unsigned long>(state.updateIntervalMs));
    }
  }
  const SystemBatteryData system = bmsManager.systemData(now);
  Serial.printf("[SYSTEM] %.2fV | %.2fA | %.0fW | %.2f/%.2fAh | SOC %.2f%%",
                system.voltage, system.current, system.power, system.remainingAh,
                system.totalCapacityAh, system.soc);
  if (system.socMismatch) Serial.printf(" | SOC DELTA %.0f%%", system.socDelta);
  Serial.println();
}

void printHeapData(uint32_t now) {
  if (static_cast<int32_t>(now - nextHeapLogMs) < 0) return;
  nextHeapLogMs = now + NetworkConfig::HEAP_LOG_INTERVAL_MS;
  Serial.printf("[SYSTEM] Free heap: %.1f KB | Min heap: %.1f KB\n",
                ESP.getFreeHeap() / 1024.0f, ESP.getMinFreeHeap() / 1024.0f);
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nESP32 BMS Gateway");
  Serial.printf("Firmware: %s\n", NetworkConfig::FIRMWARE_VERSION);
  NimBLEDevice::init("");
  NimBLEDevice::setPower(3);
  const uint32_t now = millis();
  bmsManager.begin(now);
  displayManager.begin();
  wifiManager.begin(now);
  firebaseService.begin();
}

void loop() {
  const uint32_t now = millis();
  bmsManager.update(now);
  const SystemBatteryData system = bmsManager.systemData(now);
  wifiManager.update(networkState, now);
  timeService.update(networkState);
  firebaseService.update(networkState, system, bmsManager.device(0), bmsManager.device(1),
                         now, timeService.epochMs());
  displayManager.update(system, bmsManager.device(0), bmsManager.device(1), networkState, now);
  printOperatingData(now);
  printHeapData(now);
  delay(1);
}
