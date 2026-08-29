#pragma once

#include <Arduino.h>

namespace DisplayConfig {
// Robotistan product 23452 imagery identifies the current board as E32R28T-1;
// the manufacturer document specifies ST7789P3 for this revision.
constexpr bool E32R28T_1_ST7789P3 = true;
constexpr int8_t TFT_CS = 15;
constexpr int8_t TFT_DC = 2;
constexpr int8_t TFT_RST = -1;  // Connected to ESP32 EN on this board.
constexpr int8_t TFT_SCK = 14;
constexpr int8_t TFT_MOSI = 13;
constexpr int8_t TFT_MISO = 12;
constexpr int8_t TFT_BACKLIGHT = 21;
constexpr int8_t TOUCH_SCK = 25;
constexpr int8_t TOUCH_MOSI = 32;
constexpr int8_t TOUCH_MISO = 39;
constexpr int8_t TOUCH_CS = 33;
constexpr int8_t TOUCH_IRQ = 36;
constexpr uint8_t ROTATION = 0;
constexpr bool MIRROR_HORIZONTAL = true;
constexpr uint32_t REDRAW_INTERVAL_MS = 500;
constexpr uint32_t AUTO_PAGE_INTERVAL_MS = 10000;
constexpr uint16_t BLACK = 0x0000;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t CYAN = 0x07FF;
constexpr uint16_t GREEN = 0x07E0;
constexpr uint16_t YELLOW = 0xFFE0;
constexpr uint16_t RED = 0xF800;
constexpr uint16_t DARK_GREY = 0x4208;
}  // namespace DisplayConfig
