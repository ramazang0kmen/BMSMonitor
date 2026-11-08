#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <cstring>

#include "config/DisplayConfig.h"

struct TextFieldCache {
  static constexpr uint8_t MAX_CHARS = 40;
  char text[MAX_CHARS] = {};
  uint16_t color = 0;
  bool valid = false;

  void reset() {
    memset(text, 0, sizeof(text));
    color = 0;
    valid = false;
  }
};

inline void drawChangedText(Adafruit_GFX& display, TextFieldCache& cache,
                            const char* value, int16_t x, int16_t y,
                            uint8_t size, uint16_t color, uint8_t width) {
  if (width >= TextFieldCache::MAX_CHARS) width = TextFieldCache::MAX_CHARS - 1;
  const size_t length = strlen(value);
  const bool colorChanged = !cache.valid || cache.color != color;
  display.setTextSize(size);
  display.setTextWrap(false);
  display.setTextColor(color, DisplayConfig::BLACK);
  for (uint8_t i = 0; i < width; ++i) {
    const char next = i < length ? value[i] : ' ';
    if (!cache.valid || colorChanged || cache.text[i] != next) {
      const int16_t charX = x + static_cast<int16_t>(i) * 6 * size;
      display.fillRect(charX, y, 6 * size, 8 * size, DisplayConfig::BLACK);
      if (next != ' ') {
        display.setCursor(charX, y);
        display.print(next);
      }
      cache.text[i] = next;
    }
  }
  cache.text[width] = '\0';
  cache.color = color;
  cache.valid = true;
}

inline int16_t centeredFieldX(uint8_t width, uint8_t size, int16_t screenWidth = 240) {
  return (screenWidth - static_cast<int16_t>(width) * 6 * size) / 2;
}

inline void drawCenteredChangedText(Adafruit_GFX& display, TextFieldCache& cache,
                                    const char* value, int16_t y, uint8_t size,
                                    uint16_t color, uint8_t width,
                                    int16_t screenWidth = 240) {
  if (width >= TextFieldCache::MAX_CHARS) width = TextFieldCache::MAX_CHARS - 1;
  char centered[TextFieldCache::MAX_CHARS];
  memset(centered, ' ', width);
  centered[width] = '\0';
  const size_t valueLength = strlen(value);
  const size_t copiedLength = valueLength < width ? valueLength : width;
  const size_t leftPadding = (width - copiedLength) / 2;
  memcpy(centered + leftPadding, value, copiedLength);
  drawChangedText(display, cache, centered, centeredFieldX(width, size, screenWidth),
                  y, size, color, width);
}
