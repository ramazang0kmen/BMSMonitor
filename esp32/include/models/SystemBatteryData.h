#pragma once

struct SystemBatteryData {
  float voltage = 0.0f;
  float current = 0.0f;
  float power = 0.0f;
  float remainingAh = 0.0f;
  float totalCapacityAh = 0.0f;
  float soc = 0.0f;
  bool bms1Online = false;
  bool bms2Online = false;
  uint8_t discoveredBmsCount = 0;
  uint8_t connectedBmsCount = 0;
  uint8_t onlineBmsCount = 0;
  bool socMismatch = false;
  float socDelta = 0.0f;
};
