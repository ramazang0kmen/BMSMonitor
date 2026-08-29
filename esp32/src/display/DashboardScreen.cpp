#include "display/DashboardScreen.h"

#include "config/DisplayConfig.h"

namespace {
void centerText(Adafruit_GFX& d, const char* text, int16_t y, uint8_t size, uint16_t color) {
  int16_t x1, y1; uint16_t w, h;
  d.setTextSize(size); d.setTextColor(color, DisplayConfig::BLACK);
  d.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  d.setCursor((d.width() - w) / 2, y); d.print(text);
}
}

void DashboardScreen::begin(Adafruit_GFX& d) {
  d.fillScreen(DisplayConfig::BLACK);
  centerText(d, "BATARYA SISTEMI", 5, 2, DisplayConfig::CYAN);
  d.drawFastHLine(4, 25, 232, DisplayConfig::DARK_GREY);
  d.drawRect(4, 174, 232, 50, DisplayConfig::DARK_GREY);
  d.drawRect(4, 229, 232, 50, DisplayConfig::DARK_GREY);
}

void DashboardScreen::render(Adafruit_GFX& d, const SystemBatteryData& s,
                             const BmsDeviceState& b1, const BmsDeviceState& b2,
                             const NetworkState& network) {
  char line[64];
  d.fillRect(0, 30, 240, 140, DisplayConfig::BLACK);
  snprintf(line, sizeof(line), "%%%0.1f", s.soc); centerText(d, line, 32, 5, DisplayConfig::GREEN);
  snprintf(line, sizeof(line), "%.2f V", s.voltage); centerText(d, line, 78, 3, DisplayConfig::WHITE);
  snprintf(line, sizeof(line), "%.2f A   %.0f W", s.current, s.power); centerText(d, line, 108, 2, DisplayConfig::YELLOW);
  snprintf(line, sizeof(line), "Kalan %.2f / %.0f Ah", s.remainingAh, s.totalCapacityAh);
  centerText(d, line, 137, 2, DisplayConfig::WHITE);
  d.fillRect(0, 158, 240, 12, DisplayConfig::BLACK);
  d.setTextSize(1);
  d.setCursor(73, 160);
  d.setTextColor(network.wifiConnected ? DisplayConfig::GREEN : DisplayConfig::RED,
                 DisplayConfig::BLACK);
  d.print("W");
  d.setCursor(113, 160);
  d.setTextColor(network.firebaseReady ? DisplayConfig::GREEN : DisplayConfig::RED,
                 DisplayConfig::BLACK);
  d.print("C");
  d.setCursor(153, 160);
  d.setTextColor(network.remoteViewerActive ? DisplayConfig::GREEN : DisplayConfig::DARK_GREY,
                 DisplayConfig::BLACK);
  d.print("R");

  const BmsDeviceState* states[2] = {&b1, &b2};
  for (uint8_t i = 0; i < 2; ++i) {
    const int16_t y = 178 + i * 55;
    d.fillRect(8, y, 224, 42, DisplayConfig::BLACK);
    d.setTextSize(2); d.setTextColor(DisplayConfig::CYAN, DisplayConfig::BLACK); d.setCursor(9, y); d.printf("BMS %u", i + 1);
    if (!states[i]->connected || states[i]->lastUpdateMs == 0) {
      d.setTextColor(DisplayConfig::RED, DisplayConfig::BLACK); d.setCursor(126, y); d.print("OFFLINE");
    } else {
      d.setTextColor(DisplayConfig::WHITE, DisplayConfig::BLACK); d.setCursor(126, y); d.printf("%%%u", states[i]->data.soc);
      d.setTextSize(1); d.setCursor(9, y + 23);
      d.printf("%.2fV   %.2fA   %.2fAh", states[i]->data.voltage, states[i]->data.current,
               states[i]->data.remainingCapacityAh);
    }
  }
  d.fillRect(0, 284, 240, 34, DisplayConfig::BLACK);
  if (s.socMismatch) {
    snprintf(line, sizeof(line), "SOC DELTA %.0f%%", s.socDelta);
    centerText(d, line, 291, 2, DisplayConfig::RED);
  } else centerText(d, "SOC DENGELI", 296, 1, DisplayConfig::GREEN);
}
