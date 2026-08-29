#pragma once

#include "ble/JbdBmsClient.h"
#include "models/SystemBatteryData.h"

class BmsManager {
 public:
  void begin(uint32_t now);
  void update(uint32_t now);
  SystemBatteryData systemData(uint32_t now) const;
  const BmsDeviceState& device(size_t index) const { return states_[index]; }

 private:
  void scheduleReconnect(size_t index, uint32_t now);
  BmsDeviceState states_[2];
  JbdBmsClient clients_[2];
  uint32_t nextReconnectMs_[2] = {};
  uint8_t reconnectAttempt_[2] = {};
};
