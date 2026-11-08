#include "display/DisplayManager.h"

#include "config/DisplayConfig.h"

DisplayManager::DisplayManager()
    : displaySpi_(HSPI),
      touchSpi_(VSPI),
      ili9341_(&displaySpi_, DisplayConfig::TFT_DC, DisplayConfig::TFT_CS, DisplayConfig::TFT_RST),
      st7789_(&displaySpi_, DisplayConfig::TFT_CS, DisplayConfig::TFT_DC, DisplayConfig::TFT_RST),
      touch_(DisplayConfig::TOUCH_CS, DisplayConfig::TOUCH_IRQ) {}

bool DisplayManager::begin() {
  pinMode(DisplayConfig::TFT_BACKLIGHT, OUTPUT);
  digitalWrite(DisplayConfig::TFT_BACKLIGHT, LOW);
  pinMode(DisplayConfig::TFT_CS, OUTPUT);
  pinMode(DisplayConfig::TFT_DC, OUTPUT);
  digitalWrite(DisplayConfig::TFT_CS, HIGH);
  displaySpi_.begin(DisplayConfig::TFT_SCK, DisplayConfig::TFT_MISO,
                    DisplayConfig::TFT_MOSI, DisplayConfig::TFT_CS);

  Controller controller = detectController();
  if (controller == Controller::Unknown && DisplayConfig::E32R28T_1_ST7789P3) {
    Serial.println("[DISPLAY] SPI ID read unsupported; using documented E32R28T-1 ST7789P3 configuration.");
    controller = Controller::St7789;
  }
  if (controller == Controller::Ili9341) {
    Serial.println("[DISPLAY] Controller verified: ILI9341/ILI9341V");
    ili9341_.begin(27000000);
    ili9341_.setRotation(DisplayConfig::ROTATION);
    display_ = &ili9341_;
  } else if (controller == Controller::St7789) {
    Serial.println("[DISPLAY] Controller verified: ST7789/ST7789P3");
    st7789_.init(240, 320, SPI_MODE0);
    st7789_.setRotation(DisplayConfig::ROTATION);
    if (DisplayConfig::MIRROR_HORIZONTAL && DisplayConfig::ROTATION == 0) {
      // E32R28T-1 mounts this panel with the column direction reversed.
      // Toggle MADCTL.MX while preserving portrait geometry and row direction.
      const uint8_t madctl = ST77XX_MADCTL_MY | ST77XX_MADCTL_RGB;
      st7789_.sendCommand(ST77XX_MADCTL, &madctl, 1);
      Serial.println("[DISPLAY] Horizontal mirror correction enabled");
    }
    display_ = &st7789_;
  } else {
    Serial.println("[DISPLAY] Controller ID inconclusive; TFT left disabled to avoid a guessed driver.");
    return false;
  }

  digitalWrite(DisplayConfig::TFT_BACKLIGHT, HIGH);
  initialized_ = display_->width() == 240 && display_->height() == 320;
  Serial.printf("[DISPLAY] Geometry: %dx%d portrait | %s\n", display_->width(), display_->height(),
                initialized_ ? "OK" : "ERROR");
  if (!initialized_) return false;
  dashboard_.begin(*display_);

  touchSpi_.begin(DisplayConfig::TOUCH_SCK, DisplayConfig::TOUCH_MISO,
                  DisplayConfig::TOUCH_MOSI, DisplayConfig::TOUCH_CS);
  touchInitialized_ = touch_.begin(touchSpi_);
  touch_.setRotation(DisplayConfig::ROTATION);
  Serial.printf("[TOUCH] XPT2046 pins configured | %s | calibration pending\n",
                touchInitialized_ ? "READY" : "FAILED");
  nextPageMs_ = millis() + DisplayConfig::AUTO_PAGE_INTERVAL_MS;
  return true;
}

DisplayManager::Controller DisplayManager::detectController() {
  uint8_t id4[4] = {};
  uint8_t idD3[4] = {};
  readCommand(0x04, id4, sizeof(id4));
  readCommand(0xD3, idD3, sizeof(idD3));
  Serial.printf("[DISPLAY] RDDID(04): %02X %02X %02X %02X | RDID4(D3): %02X %02X %02X %02X\n",
                id4[0], id4[1], id4[2], id4[3], idD3[0], idD3[1], idD3[2], idD3[3]);
  for (size_t i = 0; i + 1 < 4; ++i)
    if (idD3[i] == 0x93 && idD3[i + 1] == 0x41) return Controller::Ili9341;
  for (uint8_t value : id4)
    if (value == 0x85 || value == 0x52) return Controller::St7789;
  return Controller::Unknown;
}

void DisplayManager::readCommand(uint8_t command, uint8_t* result, size_t length) {
  displaySpi_.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(DisplayConfig::TFT_CS, LOW);
  digitalWrite(DisplayConfig::TFT_DC, LOW);
  displaySpi_.transfer(command);
  digitalWrite(DisplayConfig::TFT_DC, HIGH);
  for (size_t i = 0; i < length; ++i) result[i] = displaySpi_.transfer(0x00);
  digitalWrite(DisplayConfig::TFT_CS, HIGH);
  displaySpi_.endTransaction();
}

void DisplayManager::update(const SystemBatteryData& system, const BmsDeviceState* devices,
                            size_t deviceCount, size_t activeGroupStart,
                            const NetworkState& network, uint32_t now) {
  deviceCount_ = deviceCount;
  handleSerial(now);
  handleTouch(now);
  if (!initialized_) return;
  const uint8_t pageCount = static_cast<uint8_t>(deviceCount + 1);
  if (static_cast<int32_t>(now - nextPageMs_) >= 0)
    selectPage(pageCount ? (page_ + 1) % pageCount : 0, now);
  if (static_cast<int32_t>(now - nextRedrawMs_) < 0) return;
  nextRedrawMs_ = now + DisplayConfig::REDRAW_INTERVAL_MS;
  if (page_ == 0)
    dashboard_.render(*display_, system, devices, deviceCount, activeGroupStart, network, now);
  else if (page_ <= deviceCount)
    detail_.render(*display_, devices[page_ - 1], page_, now);
}

void DisplayManager::selectPage(uint8_t page, uint32_t now) {
  page_ = page > deviceCount_ ? 0 : page;
  nextPageMs_ = now + DisplayConfig::AUTO_PAGE_INTERVAL_MS;
  nextRedrawMs_ = now;
  if (!initialized_) return;
  if (page_ == 0) dashboard_.begin(*display_); else detail_.begin(*display_, page_);
  Serial.printf("[DISPLAY] Page %u selected\n", page_);
}

void DisplayManager::handleSerial(uint32_t now) {
  while (Serial.available()) {
    const char value = static_cast<char>(Serial.read());
    if (value >= '0' && value <= '2') selectPage(value - '0', now);
  }
}

void DisplayManager::handleTouch(uint32_t now) {
  if (!touchInitialized_ || !touch_.touched()) return;
  const TS_Point point = touch_.getPoint();
  if (static_cast<int32_t>(now - nextTouchLogMs_) >= 0) {
    Serial.printf("[TOUCH] raw x=%d y=%d pressure=%d (calibration required before UI mapping)\n",
                  point.x, point.y, point.z);
    nextTouchLogMs_ = now + 250U;
  }
}
