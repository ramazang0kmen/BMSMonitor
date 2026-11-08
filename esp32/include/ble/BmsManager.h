#pragma once

#include "ble/BmsScanner.h"
#include "ble/JbdBmsClient.h"
#include "config/BmsConfig.h"
#include "models/SystemBatteryData.h"

class BmsManager {
 public:
  void begin(uint32_t now);
  void update(uint32_t now);
  SystemBatteryData systemData(uint32_t now) const;

  size_t deviceCount() const { return deviceCount_; }
  const BmsDeviceState* devices() const { return states_; }
  const BmsDeviceState& device(size_t index) const;
  uint32_t dataRevision() const { return dataRevision_; }
  size_t activeGroupStart() const { return groupStart_; }
  bool rotatingGroups() const { return rotating_; }

 private:
  static String makeId(const String& mac);
  void discoverDevices();
  void initializeTestDevices();
  void updateTestDevices(uint32_t now);
  void activateGroup(uint32_t now);
  void scheduleReconnect(size_t slot, uint32_t now);
  void advanceGroup(uint32_t now);

  BmsScanner scanner_;
  BmsDeviceState states_[BmsConfig::MAX_TRACKED_BMS];
  JbdBmsClient clients_[BmsConfig::MAX_ACTIVE_CONNECTIONS];
  size_t slotDevice_[BmsConfig::MAX_ACTIVE_CONNECTIONS] = {SIZE_MAX, SIZE_MAX, SIZE_MAX};
  uint32_t nextReconnectMs_[BmsConfig::MAX_ACTIVE_CONNECTIONS] = {};
  uint32_t observedUpdateMs_[BmsConfig::MAX_ACTIVE_CONNECTIONS] = {};
  uint8_t reconnectAttempt_[BmsConfig::MAX_ACTIVE_CONNECTIONS] = {};
  size_t deviceCount_ = 0;
  size_t groupStart_ = 0;
  uint32_t groupStartedMs_ = 0;
  uint32_t groupSwitchAtMs_ = 0;
  uint32_t dataRevision_ = 0;
  bool rotating_ = false;
  bool testMode_ = false;
  bool switchingGroup_ = false;
  uint32_t nextTestUpdateMs_ = 0;
};
