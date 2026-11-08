#include "display/BmsDetailScreen.h"

#include "config/DisplayConfig.h"

void BmsDetailScreen::resetCaches() {
  for (auto& field : fields_) field.reset();
}

void BmsDetailScreen::begin(Adafruit_GFX& display, uint8_t number) {
  resetCaches();
  display.fillScreen(DisplayConfig::BLACK);
  display.setTextSize(2);
  display.setTextColor(DisplayConfig::CYAN, DisplayConfig::BLACK);
  display.setCursor(8, 5);
  display.printf("BMS %u DETAIL", number);
  display.drawFastHLine(4, 25, 232, DisplayConfig::DARK_GREY);
}

void BmsDetailScreen::render(Adafruit_GFX& display, const BmsDeviceState& state,
                             uint8_t number, uint32_t now) {
  (void)number;
  char text[64];
  const bool online = state.hasFreshData(now);

  snprintf(text, sizeof(text), "%%%u  %s", state.data.soc,
           online ? (state.connected ? "ONLINE" : "BEKLEME") : "OFFLINE");
  drawChangedText(display, fields_[0], text, 8, 32, 2,
                  online ? (state.connected ? DisplayConfig::GREEN : DisplayConfig::YELLOW)
                         : DisplayConfig::RED,
                  18);

  snprintf(text, sizeof(text), "%.2fV %.2fA", state.data.voltage, state.data.current);
  drawChangedText(display, fields_[1], text, 8, 54, 2, DisplayConfig::WHITE, 18);
  snprintf(text, sizeof(text), "Power %.1fW  %.2f/%.1fAh", state.data.power,
           state.data.remainingCapacityAh, state.data.nominalCapacityAh);
  drawChangedText(display, fields_[2], text, 8, 76, 1, DisplayConfig::WHITE, 36);

  const uint8_t cellCount = state.data.cellCount > 8 ? 8 : state.data.cellCount;
  for (uint8_t i = 0; i < 8; ++i) {
    const int16_t y = 96 + i * 16;
    if (i >= cellCount) {
      drawChangedText(display, fields_[3 + i], "", 12, y, 1,
                      DisplayConfig::WHITE, 17);
      continue;
    }
    const bool isMin = state.data.cellVoltages[i] == state.data.minCellVoltage;
    const bool isMax = state.data.cellVoltages[i] == state.data.maxCellVoltage;
    snprintf(text, sizeof(text), "C%u  %.3f V %s", i + 1, state.data.cellVoltages[i],
             isMin ? "<" : (isMax ? ">" : ""));
    drawChangedText(display, fields_[3 + i], text, 12, y, 1,
                    isMin ? DisplayConfig::YELLOW
                          : (isMax ? DisplayConfig::CYAN : DisplayConfig::WHITE),
                    17);
  }

  snprintf(text, sizeof(text), "MIN %.3f", state.data.minCellVoltage);
  drawChangedText(display, fields_[11], text, 120, 96, 1, DisplayConfig::WHITE, 18);
  snprintf(text, sizeof(text), "MAX %.3f", state.data.maxCellVoltage);
  drawChangedText(display, fields_[12], text, 120, 112, 1, DisplayConfig::WHITE, 18);
  snprintf(text, sizeof(text), "D %.0fmV", state.data.cellDelta * 1000.0f);
  drawChangedText(display, fields_[13], text, 120, 128, 1, DisplayConfig::WHITE, 18);
  snprintf(text, sizeof(text), "RSSI %d", state.rssi);
  drawChangedText(display, fields_[14], text, 120, 152, 1, DisplayConfig::WHITE, 18);
  snprintf(text, sizeof(text), "ERR %lu", static_cast<unsigned long>(state.errorCount));
  drawChangedText(display, fields_[15], text, 120, 168, 1, DisplayConfig::WHITE, 18);

  size_t used = snprintf(text, sizeof(text), "TEMP ");
  for (uint8_t i = 0; i < state.data.temperatureCount && i < 3 && used < sizeof(text); ++i) {
    used += snprintf(text + used, sizeof(text) - used, "%.1f%s", state.data.temperatures[i],
                     i + 1 < state.data.temperatureCount && i < 2 ? "/" : "C");
  }
  drawChangedText(display, fields_[16], text, 8, 232, 1, DisplayConfig::WHITE, 36);

  snprintf(text, sizeof(text), "CHG %s  DSG %s", state.data.chargeMos ? "ON" : "OFF",
           state.data.dischargeMos ? "ON" : "OFF");
  drawChangedText(display, fields_[17], text, 8, 252, 1, DisplayConfig::WHITE, 36);
  snprintf(text, sizeof(text), "BAL %s  PROT %04X", state.data.balancing ? "ON" : "OFF",
           state.data.protectionFlags);
  drawChangedText(display, fields_[18], text, 8, 270, 1, DisplayConfig::WHITE, 36);
  snprintf(text, sizeof(text), "ID %s", state.id.c_str());
  drawChangedText(display, fields_[19], text, 8, 302, 1, DisplayConfig::DARK_GREY, 38);
}
