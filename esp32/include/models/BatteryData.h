#pragma once

#include <Arduino.h>
#include <string>

struct BatteryData {
  float voltage = 0.0f;
  float current = 0.0f;
  float power = 0.0f;
  float remainingCapacityAh = 0.0f;
  float nominalCapacityAh = 0.0f;
  uint8_t soc = 0;

  float temperatures[8] = {};
  uint8_t temperatureCount = 0;

  float cellVoltages[32] = {};
  uint8_t cellCount = 0;

  float minCellVoltage = 0.0f;
  float maxCellVoltage = 0.0f;
  float cellDelta = 0.0f;

  bool chargeMos = false;
  bool dischargeMos = false;
  bool balancing = false;
  uint16_t protectionFlags = 0;
};

struct BmsDevice {
  std::string name;
  std::string mac;
  uint8_t addressType = 0;
  BatteryData data;
  bool connected = false;
  int rssi = 0;
};

struct BmsDeviceState {
  String id;
  String name;
  String mac;
  uint8_t addressType = 0;
  uint16_t order = 0;
  bool connected = false;
  bool discovered = false;
  int rssi = 0;
  uint32_t lastUpdateMs = 0;
  uint32_t updateIntervalMs = 0;
  uint32_t staleAfterMs = 5000;
  uint32_t errorCount = 0;
  BatteryData data;

  bool hasFreshData(uint32_t now) const {
    return lastUpdateMs != 0 && now - lastUpdateMs <= staleAfterMs;
  }
};
