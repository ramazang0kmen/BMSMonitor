#pragma once

#include <Arduino.h>

#include "models/BatteryData.h"
#include "models/NetworkState.h"
#include "models/SystemBatteryData.h"

class EventLogMonitor;

class FirebaseService {
 public:
  FirebaseService();
  ~FirebaseService();
  void begin();
  void update(NetworkState& network, const SystemBatteryData& system,
              const BmsDeviceState& bms1, const BmsDeviceState& bms2,
              EventLogMonitor& eventLog, uint32_t now, uint64_t epochMs);
  size_t lastLivePayloadSize() const;

 private:
  struct Impl;
  Impl* impl_;
};
