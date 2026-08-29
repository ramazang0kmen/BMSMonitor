#pragma once

#include <NimBLEDevice.h>
#include <string>
#include <vector>

#include "models/BatteryData.h"

struct DiscoveredBleDevice {
  BmsDevice bms;
  std::vector<std::string> services;
  bool possibleBms = false;
};
class BmsScanner : public NimBLEScanCallbacks {
 public:
  void begin();
  void scan(uint32_t durationMs);
  void printCandidateSummary() const;
  const std::vector<DiscoveredBleDevice>& devices() const { return devices_; }

 private:
  void onResult(const NimBLEAdvertisedDevice* device) override;
  static bool looksLikeJbd(const NimBLEAdvertisedDevice& device);
  static void printHex(const uint8_t* data, size_t length);
  static const char* addressTypeName(uint8_t type);

  std::vector<DiscoveredBleDevice> devices_;
};
