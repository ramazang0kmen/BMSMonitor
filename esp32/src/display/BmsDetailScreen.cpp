#include "display/BmsDetailScreen.h"

#include "config/DisplayConfig.h"

void BmsDetailScreen::begin(Adafruit_GFX& d, uint8_t number) {
  d.fillScreen(DisplayConfig::BLACK);
  d.setTextSize(2); d.setTextColor(DisplayConfig::CYAN); d.setCursor(8, 5); d.printf("BMS %u DETAIL", number);
  d.drawFastHLine(4, 25, 232, DisplayConfig::DARK_GREY);
}

void BmsDetailScreen::render(Adafruit_GFX& d, const BmsDeviceState& s, uint8_t number) {
  d.fillRect(0, 30, 240, 288, DisplayConfig::BLACK);
  d.setTextColor(s.connected ? DisplayConfig::GREEN : DisplayConfig::RED); d.setTextSize(2); d.setCursor(8, 32);
  d.printf("%%%u  %s", s.data.soc, s.connected ? "ONLINE" : "OFFLINE");
  d.setTextColor(DisplayConfig::WHITE); d.setCursor(8, 54); d.printf("%.2fV %.2fA", s.data.voltage, s.data.current);
  d.setTextSize(1); d.setCursor(8, 76); d.printf("Power %.1fW  %.2f/%.1fAh", s.data.power,
      s.data.remainingCapacityAh, s.data.nominalCapacityAh);
  const uint8_t cells = s.data.cellCount > 8 ? 8 : s.data.cellCount;
  for (uint8_t i = 0; i < cells; ++i) {
    const int16_t y = 96 + i * 16;
    d.setCursor(12, y); d.setTextSize(1);
    const bool isMin = s.data.cellVoltages[i] == s.data.minCellVoltage;
    const bool isMax = s.data.cellVoltages[i] == s.data.maxCellVoltage;
    d.setTextColor(isMin ? DisplayConfig::YELLOW : (isMax ? DisplayConfig::CYAN : DisplayConfig::WHITE));
    d.printf("C%u  %.3f V %s", i + 1, s.data.cellVoltages[i], isMin ? "<" : (isMax ? ">" : ""));
  }
  d.setTextColor(DisplayConfig::WHITE); d.setCursor(120, 96); d.printf("MIN %.3f", s.data.minCellVoltage);
  d.setCursor(120, 112); d.printf("MAX %.3f", s.data.maxCellVoltage);
  d.setCursor(120, 128); d.printf("D %.0fmV", s.data.cellDelta * 1000.0f);
  d.setCursor(120, 152); d.printf("RSSI %d", s.rssi);
  d.setCursor(120, 168); d.printf("ERR %lu", static_cast<unsigned long>(s.errorCount));
  d.setCursor(8, 232); d.print("TEMP ");
  for (uint8_t i = 0; i < s.data.temperatureCount && i < 3; ++i) d.printf("%.1f%s", s.data.temperatures[i], i + 1 < s.data.temperatureCount && i < 2 ? "/" : "C");
  d.setCursor(8, 252); d.printf("CHG %s  DSG %s", s.data.chargeMos ? "ON" : "OFF", s.data.dischargeMos ? "ON" : "OFF");
  d.setCursor(8, 270); d.printf("BAL %s  PROT %04X", s.data.balancing ? "ON" : "OFF", s.data.protectionFlags);
  d.setTextColor(DisplayConfig::DARK_GREY); d.setCursor(8, 302); d.printf("Serial: 0 ana, 1/2 detay");
}
