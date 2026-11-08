#pragma once

#include <Adafruit_GFX.h>
#include "display/IncrementalText.h"
#include "models/BatteryData.h"

class BmsDetailScreen {
 public:
  void begin(Adafruit_GFX& display, uint8_t number);
  void render(Adafruit_GFX& display, const BmsDeviceState& state, uint8_t number, uint32_t now);

 private:
  void resetCaches();
  TextFieldCache fields_[20];
};
