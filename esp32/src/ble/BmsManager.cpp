#include "ble/BmsManager.h"

#include <algorithm>
#include <cmath>

#include "config/BmsConfig.h"

void BmsManager::begin(uint32_t now) {
  for (size_t i = 0; i < 2; ++i) {
    states_[i].name = BmsConfig::DEVICES[i].name;
    states_[i].mac = BmsConfig::DEVICES[i].mac;
    clients_[i].begin(states_[i]);
    // ESP32 can keep two links open, but NimBLE starts one connection procedure
    // at a time. Stagger only the initial connection establishment.
    nextReconnectMs_[i] = now + static_cast<uint32_t>(i) * 1000U;
  }
}

void BmsManager::update(uint32_t now) {
  for (size_t i = 0; i < 2; ++i) {
    clients_[i].update(now);
    if (clients_[i].consumeFailureEvent()) scheduleReconnect(i, now);
    if (clients_[i].isDisconnected() && static_cast<int32_t>(now - nextReconnectMs_[i]) >= 0) {
      if (!clients_[i].startConnect(now)) scheduleReconnect(i, now);
    }
    if (states_[i].connected && states_[i].lastUpdateMs != 0) reconnectAttempt_[i] = 0;
  }
}

void BmsManager::scheduleReconnect(size_t index, uint32_t now) {
  const size_t slot = std::min<size_t>(reconnectAttempt_[index], 2U);
  nextReconnectMs_[index] = now + BmsConfig::RECONNECT_BACKOFF_MS[slot];
  if (reconnectAttempt_[index] < 2) ++reconnectAttempt_[index];
  Serial.printf("[INFO] %s reconnect in %lu ms\n", states_[index].name,
                static_cast<unsigned long>(BmsConfig::RECONNECT_BACKOFF_MS[slot]));
}

SystemBatteryData BmsManager::systemData(uint32_t now) const {
  SystemBatteryData system;
  const bool online[2] = {
      states_[0].connected && states_[0].lastUpdateMs != 0 && now - states_[0].lastUpdateMs <= BmsConfig::DATA_STALE_MS,
      states_[1].connected && states_[1].lastUpdateMs != 0 && now - states_[1].lastUpdateMs <= BmsConfig::DATA_STALE_MS,
  };
  system.bms1Online = online[0];
  system.bms2Online = online[1];
  float voltageSum = 0.0f;
  uint8_t onlineCount = 0;
  for (size_t i = 0; i < 2; ++i) {
    if (!online[i]) continue;
    const BatteryData& data = states_[i].data;
    voltageSum += data.voltage;
    ++onlineCount;
    system.current += data.current;
    system.power += data.voltage * data.current;
    system.remainingAh += data.remainingCapacityAh;
    system.totalCapacityAh += data.nominalCapacityAh;
  }
  if (onlineCount) system.voltage = voltageSum / onlineCount;
  if (system.totalCapacityAh > 0.0f) system.soc = system.remainingAh / system.totalCapacityAh * 100.0f;
  if (online[0] && online[1]) {
    system.socDelta = std::fabs(static_cast<float>(states_[0].data.soc) - states_[1].data.soc);
    system.socMismatch = system.socDelta >= 10.0f;
  }
  return system;
}
