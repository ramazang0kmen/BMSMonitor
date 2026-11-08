#pragma once

#include <Adafruit_GFX.h>

#include "display/IncrementalText.h"
#include "models/BatteryData.h"
#include "models/NetworkState.h"
#include "models/SystemBatteryData.h"

class DashboardScreen {
 public:
  void begin(Adafruit_GFX& display);
  void render(Adafruit_GFX& display, const SystemBatteryData& system,
              const BmsDeviceState* devices, size_t deviceCount, size_t listStart,
              const NetworkState& network, uint32_t now);

 private:
  void resetCaches();
  TextFieldCache fields_[18];
};
