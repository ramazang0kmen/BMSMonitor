#pragma once

#include <Adafruit_GFX.h>

#include "models/BatteryData.h"
#include "models/NetworkState.h"
#include "models/SystemBatteryData.h"

class DashboardScreen {
 public:
  void begin(Adafruit_GFX& display);
  void render(Adafruit_GFX& display, const SystemBatteryData& system,
              const BmsDeviceState& bms1, const BmsDeviceState& bms2,
              const NetworkState& network);
};
