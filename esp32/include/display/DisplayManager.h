#pragma once

#include <Adafruit_ILI9341.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

#include "display/BmsDetailScreen.h"
#include "display/DashboardScreen.h"
#include "models/SystemBatteryData.h"
#include "models/NetworkState.h"

class DisplayManager {
 public:
  DisplayManager();
  bool begin();
  void update(const SystemBatteryData& system, const BmsDeviceState& bms1,
              const BmsDeviceState& bms2, const NetworkState& network, uint32_t now);

 private:
  enum class Controller : uint8_t { Unknown, Ili9341, St7789 };
  Controller detectController();
  void readCommand(uint8_t command, uint8_t* result, size_t length);
  void selectPage(uint8_t page, uint32_t now);
  void handleSerial(uint32_t now);
  void handleTouch(uint32_t now);

  SPIClass displaySpi_;
  SPIClass touchSpi_;
  Adafruit_ILI9341 ili9341_;
  Adafruit_ST7789 st7789_;
  XPT2046_Touchscreen touch_;
  Adafruit_GFX* display_ = nullptr;
  DashboardScreen dashboard_;
  BmsDetailScreen detail_;
  uint8_t page_ = 0;
  bool initialized_ = false;
  bool touchInitialized_ = false;
  uint32_t nextRedrawMs_ = 0;
  uint32_t nextPageMs_ = 0;
  uint32_t nextTouchLogMs_ = 0;
};
