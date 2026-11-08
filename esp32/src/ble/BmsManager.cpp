#include "ble/BmsManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <vector>

namespace {
bool newerOrEqual(uint32_t value, uint32_t baseline) {
  return value != 0 && static_cast<int32_t>(value - baseline) >= 0;
}
}  // namespace

String BmsManager::makeId(const String& mac) {
  String id = "bms_";
  id.reserve(mac.length() + 4);
  for (size_t i = 0; i < mac.length(); ++i) {
    const char c = mac[i];
    if (isalnum(static_cast<unsigned char>(c))) id += static_cast<char>(tolower(c));
  }
  return id;
}

void BmsManager::begin(uint32_t now) {
  testMode_ = BmsConfig::TEST_BMS_MODE;
  if (testMode_) {
    initializeTestDevices();
    Serial.println("[BMS][TEST] Synthetic nine-BMS mode enabled; BLE scan skipped");
  } else {
    scanner_.begin();
    scanner_.scan(BmsConfig::SCAN_DURATION_MS);
    discoverDevices();
  }
  rotating_ = deviceCount_ > BmsConfig::MAX_ACTIVE_CONNECTIONS;
  for (size_t i = 0; i < deviceCount_; ++i) {
    states_[i].staleAfterMs = rotating_ ? BmsConfig::ROTATING_DATA_STALE_MS
                                        : BmsConfig::DATA_STALE_MS;
  }
  Serial.printf("[BMS] %u device(s) tracked | mode=%s | group size=%u\n",
                static_cast<unsigned>(deviceCount_), rotating_ ? "ROTATING" : "PERSISTENT",
                static_cast<unsigned>(BmsConfig::MAX_ACTIVE_CONNECTIONS));
  activateGroup(now);
}

void BmsManager::initializeTestDevices() {
  deviceCount_ = std::min(BmsConfig::TEST_BMS_COUNT, BmsConfig::MAX_TRACKED_BMS);
  for (size_t i = 0; i < deviceCount_; ++i) {
    BmsDeviceState& state = states_[i];
    char id[20];
    char name[20];
    char mac[18];
    snprintf(id, sizeof(id), "test_bms_%02u", static_cast<unsigned>(i + 1));
    snprintf(name, sizeof(name), "TEST BMS %u", static_cast<unsigned>(i + 1));
    snprintf(mac, sizeof(mac), "02:00:00:00:00:%02X", static_cast<unsigned>(i + 1));
    state.id = id;
    state.name = name;
    state.mac = mac;
    state.order = static_cast<uint16_t>(i);
    state.discovered = true;
    state.rssi = -48 - static_cast<int>(i * 3);
  }
}

void BmsManager::updateTestDevices(uint32_t now) {
  if (static_cast<int32_t>(now - nextTestUpdateMs_) < 0) return;
  nextTestUpdateMs_ = now + BmsConfig::TEST_UPDATE_INTERVAL_MS;
  const float phase = static_cast<float>(now % 60000U) / 60000.0f * 6.2831853f;
  const size_t end = std::min(deviceCount_, groupStart_ + BmsConfig::MAX_ACTIVE_CONNECTIONS);
  for (size_t i = groupStart_; i < end; ++i) {
    BmsDeviceState& state = states_[i];
    BatteryData& data = state.data;
    const uint32_t previousUpdate = state.lastUpdateMs;
    const float ripple = sinf(phase + static_cast<float>(i) * 0.37f) * 0.025f;
    data.cellCount = 8;
    data.minCellVoltage = 10.0f;
    data.maxCellVoltage = 0.0f;
    data.voltage = 0.0f;
    for (uint8_t cell = 0; cell < data.cellCount; ++cell) {
      const float cellVoltage = 3.245f + static_cast<float>(i) * 0.003f +
                                static_cast<float>(cell % 3) * 0.001f + ripple / 8.0f;
      data.cellVoltages[cell] = cellVoltage;
      data.voltage += cellVoltage;
      data.minCellVoltage = std::min(data.minCellVoltage, cellVoltage);
      data.maxCellVoltage = std::max(data.maxCellVoltage, cellVoltage);
    }
    data.cellDelta = data.maxCellVoltage - data.minCellVoltage;
    data.current = -1.2f - static_cast<float>(i) * 0.35f + sinf(phase) * 0.15f;
    data.power = data.voltage * data.current;
    data.soc = static_cast<uint8_t>(42 + i * 4);
    data.nominalCapacityAh = 100.0f;
    data.remainingCapacityAh = static_cast<float>(data.soc);
    data.temperatureCount = 3;
    data.temperatures[0] = 23.5f + static_cast<float>(i) * 0.4f;
    data.temperatures[1] = data.temperatures[0] + 0.6f;
    data.temperatures[2] = data.temperatures[0] - 0.3f;
    data.chargeMos = true;
    data.dischargeMos = true;
    data.balancing = (i % 3) == 0;
    data.protectionFlags = 0;
    state.updateIntervalMs = previousUpdate == 0 ? 0 : now - previousUpdate;
    state.lastUpdateMs = now;
    state.connected = true;
    ++dataRevision_;
  }
}

void BmsManager::discoverDevices() {
  struct Candidate {
    String name;
    String mac;
    uint8_t addressType;
    int rssi;
  };
  std::vector<Candidate> candidates;
  candidates.reserve(scanner_.devices().size());
  for (const auto& found : scanner_.devices()) {
    if (!found.possibleBms) continue;
    candidates.push_back({found.bms.name.c_str(), found.bms.mac.c_str(),
                          found.bms.addressType, found.bms.rssi});
  }
  std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
    const int byName = a.name.compareTo(b.name);
    return byName == 0 ? a.mac.compareTo(b.mac) < 0 : byName < 0;
  });

  for (const Candidate& candidate : candidates) {
    if (deviceCount_ >= BmsConfig::MAX_TRACKED_BMS) break;
    BmsDeviceState& state = states_[deviceCount_];
    state.id = makeId(candidate.mac);
    state.name = candidate.name.length() ? candidate.name : String("BMS ") + (deviceCount_ + 1);
    state.mac = candidate.mac;
    state.addressType = candidate.addressType;
    state.rssi = candidate.rssi;
    state.order = static_cast<uint16_t>(deviceCount_);
    state.discovered = true;
    ++deviceCount_;
  }

  if (deviceCount_ == 0) {
    Serial.println("[BMS][WARN] No JBD advertisement found; using configured fallback devices");
    for (const auto& fallback : BmsConfig::FALLBACK_DEVICES) {
      if (deviceCount_ >= BmsConfig::MAX_TRACKED_BMS) break;
      BmsDeviceState& state = states_[deviceCount_];
      state.id = makeId(fallback.mac);
      state.name = fallback.name;
      state.mac = fallback.mac;
      state.addressType = 0;
      state.order = static_cast<uint16_t>(deviceCount_);
      ++deviceCount_;
    }
  }
}

void BmsManager::activateGroup(uint32_t now) {
  switchingGroup_ = false;
  groupStartedMs_ = now;
  if (testMode_) {
    for (size_t i = 0; i < deviceCount_; ++i) states_[i].connected = false;
    const size_t end = std::min(deviceCount_, groupStart_ + BmsConfig::MAX_ACTIVE_CONNECTIONS);
    for (size_t i = groupStart_; i < end; ++i) states_[i].connected = true;
    nextTestUpdateMs_ = now;
    Serial.printf("[BMS][TEST] Active synthetic group %u-%u of %u\n",
                  static_cast<unsigned>(groupStart_ + 1), static_cast<unsigned>(end),
                  static_cast<unsigned>(deviceCount_));
    return;
  }
  for (size_t slot = 0; slot < BmsConfig::MAX_ACTIVE_CONNECTIONS; ++slot) {
    clients_[slot].release();
    slotDevice_[slot] = SIZE_MAX;
    reconnectAttempt_[slot] = 0;
    observedUpdateMs_[slot] = 0;
    const size_t index = groupStart_ + slot;
    if (index >= deviceCount_) continue;
    slotDevice_[slot] = index;
    observedUpdateMs_[slot] = states_[index].lastUpdateMs;
    nextReconnectMs_[slot] = now + static_cast<uint32_t>(slot) * 900U;
    if (!clients_[slot].attach(states_[index])) {
      Serial.printf("[BMS][ERROR] Could not assign client slot %u to %s\n",
                    static_cast<unsigned>(slot), states_[index].name.c_str());
      scheduleReconnect(slot, now);
    }
  }
  const size_t end = std::min(deviceCount_, groupStart_ + BmsConfig::MAX_ACTIVE_CONNECTIONS);
  Serial.printf("[BMS] Active group %u-%u of %u\n", static_cast<unsigned>(groupStart_ + 1),
                static_cast<unsigned>(end), static_cast<unsigned>(deviceCount_));
}

void BmsManager::update(uint32_t now) {
  if (switchingGroup_) {
    if (static_cast<int32_t>(now - groupSwitchAtMs_) >= 0) activateGroup(now);
    return;
  }

  if (testMode_) {
    updateTestDevices(now);
    if (now - groupStartedMs_ >= BmsConfig::GROUP_MIN_DWELL_MS) advanceGroup(now);
    return;
  }

  bool allSampled = true;
  for (size_t slot = 0; slot < BmsConfig::MAX_ACTIVE_CONNECTIONS; ++slot) {
    const size_t index = slotDevice_[slot];
    if (index == SIZE_MAX) continue;
    JbdBmsClient& client = clients_[slot];
    client.update(now);
    if (client.consumeFailureEvent()) scheduleReconnect(slot, now);
    if (client.isDisconnected() && static_cast<int32_t>(now - nextReconnectMs_[slot]) >= 0) {
      if (!client.startConnect(now)) scheduleReconnect(slot, now);
    }
    BmsDeviceState& state = states_[index];
    if (state.lastUpdateMs != observedUpdateMs_[slot]) {
      observedUpdateMs_[slot] = state.lastUpdateMs;
      ++dataRevision_;
      reconnectAttempt_[slot] = 0;
    }
    if (!newerOrEqual(state.lastUpdateMs, groupStartedMs_)) allSampled = false;
  }

  if (!rotating_) return;
  const uint32_t elapsed = now - groupStartedMs_;
  if (elapsed >= BmsConfig::GROUP_MIN_DWELL_MS &&
      (allSampled || elapsed >= BmsConfig::GROUP_MAX_DWELL_MS)) {
    advanceGroup(now);
  }
}

void BmsManager::advanceGroup(uint32_t now) {
  for (auto& client : clients_) client.release();
  if (testMode_) {
    for (size_t i = 0; i < deviceCount_; ++i) states_[i].connected = false;
  }
  groupStart_ += BmsConfig::MAX_ACTIVE_CONNECTIONS;
  if (groupStart_ >= deviceCount_) groupStart_ = 0;
  switchingGroup_ = true;
  groupSwitchAtMs_ = now + BmsConfig::GROUP_SWITCH_DELAY_MS;
}

void BmsManager::scheduleReconnect(size_t slot, uint32_t now) {
  if (slot >= BmsConfig::MAX_ACTIVE_CONNECTIONS || slotDevice_[slot] == SIZE_MAX) return;
  const size_t last = sizeof(BmsConfig::RECONNECT_BACKOFF_MS) /
                      sizeof(BmsConfig::RECONNECT_BACKOFF_MS[0]) - 1;
  const size_t backoffSlot = std::min<size_t>(reconnectAttempt_[slot], last);
  nextReconnectMs_[slot] = now + BmsConfig::RECONNECT_BACKOFF_MS[backoffSlot];
  if (reconnectAttempt_[slot] < last) ++reconnectAttempt_[slot];
  Serial.printf("[INFO] %s reconnect in %lu ms\n", states_[slotDevice_[slot]].name.c_str(),
                static_cast<unsigned long>(BmsConfig::RECONNECT_BACKOFF_MS[backoffSlot]));
}

const BmsDeviceState& BmsManager::device(size_t index) const {
  static const BmsDeviceState empty;
  return index < deviceCount_ ? states_[index] : empty;
}

SystemBatteryData BmsManager::systemData(uint32_t now) const {
  SystemBatteryData system;
  system.discoveredBmsCount = static_cast<uint8_t>(deviceCount_);
  float voltageSum = 0.0f;
  float minSoc = 101.0f;
  float maxSoc = -1.0f;
  for (size_t i = 0; i < deviceCount_; ++i) {
    const BmsDeviceState& state = states_[i];
    if (state.connected) ++system.connectedBmsCount;
    const bool online = state.hasFreshData(now);
    if (i == 0) system.bms1Online = online;
    if (i == 1) system.bms2Online = online;
    if (!online) continue;
    ++system.onlineBmsCount;
    const BatteryData& data = state.data;
    voltageSum += data.voltage;
    system.current += data.current;
    system.power += data.voltage * data.current;
    system.remainingAh += data.remainingCapacityAh;
    system.totalCapacityAh += data.nominalCapacityAh;
    minSoc = std::min(minSoc, static_cast<float>(data.soc));
    maxSoc = std::max(maxSoc, static_cast<float>(data.soc));
  }
  if (system.onlineBmsCount) system.voltage = voltageSum / system.onlineBmsCount;
  if (system.totalCapacityAh > 0.0f)
    system.soc = system.remainingAh / system.totalCapacityAh * 100.0f;
  if (system.onlineBmsCount >= 2) {
    system.socDelta = maxSoc - minSoc;
    system.socMismatch = system.socDelta >= 10.0f;
  }
  return system;
}
