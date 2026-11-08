#include "display/DashboardScreen.h"

#include "config/DisplayConfig.h"

namespace {
void centerStaticText(Adafruit_GFX& display, const char* text, int16_t y,
                      uint8_t size, uint16_t color) {
  int16_t x1, y1;
  uint16_t width, height;
  display.setTextSize(size);
  display.setTextColor(color, DisplayConfig::BLACK);
  display.getTextBounds(text, 0, y, &x1, &y1, &width, &height);
  display.setCursor((display.width() - width) / 2, y);
  display.print(text);
}
}  // namespace

void DashboardScreen::resetCaches() {
  for (auto& field : fields_) field.reset();
}

void DashboardScreen::begin(Adafruit_GFX& display) {
  resetCaches();
  display.fillScreen(DisplayConfig::BLACK);
  centerStaticText(display, "BATARYA SISTEMI", 5, 2, DisplayConfig::CYAN);
  display.drawFastHLine(4, 25, 232, DisplayConfig::DARK_GREY);
  for (uint8_t row = 0; row < 3; ++row) {
    display.drawRect(4, 188 + row * 30, 232, 28, DisplayConfig::DARK_GREY);
  }
}

void DashboardScreen::render(Adafruit_GFX& display, const SystemBatteryData& system,
                             const BmsDeviceState* devices, size_t deviceCount,
                             size_t listStart, const NetworkState& network, uint32_t now) {
  char text[64];

  snprintf(text, sizeof(text), "%%%0.1f", system.soc);
  drawCenteredChangedText(display, fields_[0], text, 32, 5, DisplayConfig::GREEN, 8);
  snprintf(text, sizeof(text), "%.2f V", system.voltage);
  drawChangedText(display, fields_[1], text, centeredFieldX(10, 3), 78, 3,
                  DisplayConfig::WHITE, 10);
  snprintf(text, sizeof(text), "%.2f A   %.0f W", system.current, system.power);
  drawChangedText(display, fields_[2], text, centeredFieldX(18, 2), 108, 2,
                  DisplayConfig::YELLOW, 18);
  snprintf(text, sizeof(text), "Kalan %.2f / %.0f Ah", system.remainingAh,
           system.totalCapacityAh);
  drawChangedText(display, fields_[3], text, centeredFieldX(20, 2), 137, 2,
                  DisplayConfig::WHITE, 20);

  drawChangedText(display, fields_[4], "W", 73, 160, 1,
                  network.wifiConnected ? DisplayConfig::GREEN : DisplayConfig::RED, 1);
  drawChangedText(display, fields_[5], "C", 113, 160, 1,
                  network.firebaseReady ? DisplayConfig::GREEN : DisplayConfig::RED, 1);
  drawChangedText(display, fields_[6], "R", 153, 160, 1,
                  network.remoteSessionActive ? DisplayConfig::GREEN : DisplayConfig::DARK_GREY,
                  1);

  snprintf(text, sizeof(text), "BMS %u  bagli %u  guncel %u",
           system.discoveredBmsCount, system.connectedBmsCount, system.onlineBmsCount);
  drawChangedText(display, fields_[7], text, 8, 174, 1, DisplayConfig::DARK_GREY, 37);

  if (deviceCount <= 3) listStart = 0;
  for (uint8_t row = 0; row < 3; ++row) {
    const size_t index = listStart + row;
    const uint8_t cache = 8 + row * 3;
    const int16_t y = 190 + row * 30;
    if (index >= deviceCount) {
      drawChangedText(display, fields_[cache], "", 8, y, 1, DisplayConfig::CYAN, 26);
      drawChangedText(display, fields_[cache + 1], "", 174, y, 1, DisplayConfig::RED, 9);
      drawChangedText(display, fields_[cache + 2], "", 8, y + 13, 1,
                      DisplayConfig::WHITE, 35);
      continue;
    }

    const BmsDeviceState& state = devices[index];
    snprintf(text, sizeof(text), "B%u %.18s", static_cast<unsigned>(index + 1),
             state.name.c_str());
    drawChangedText(display, fields_[cache], text, 8, y, 1, DisplayConfig::CYAN, 26);

    const bool online = state.hasFreshData(now);
    if (!online) {
      drawChangedText(display, fields_[cache + 1], "OFF", 174, y, 1,
                      DisplayConfig::RED, 9);
      drawChangedText(display, fields_[cache + 2], "", 8, y + 13, 1,
                      DisplayConfig::WHITE, 35);
    } else {
      snprintf(text, sizeof(text), "%%%u", state.data.soc);
      drawChangedText(display, fields_[cache + 1], text, 174, y, 1,
                      state.connected ? DisplayConfig::GREEN : DisplayConfig::YELLOW, 9);
      snprintf(text, sizeof(text), "%.2fV  %.2fA  %lus", state.data.voltage,
               state.data.current,
               static_cast<unsigned long>((now - state.lastUpdateMs) / 1000U));
      drawChangedText(display, fields_[cache + 2], text, 8, y + 13, 1,
                      DisplayConfig::WHITE, 35);
    }
  }

  if (system.socMismatch) {
    snprintf(text, sizeof(text), "SOC DELTA %.0f%%", system.socDelta);
    drawChangedText(display, fields_[17], text, centeredFieldX(18, 2), 291, 2,
                    DisplayConfig::RED, 18);
  } else {
    drawChangedText(display, fields_[17], "SOC DENGELI", centeredFieldX(18, 2), 291, 2,
                    DisplayConfig::GREEN, 18);
  }
}
