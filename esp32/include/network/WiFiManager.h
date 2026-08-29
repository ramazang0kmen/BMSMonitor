#pragma once

#include <Arduino.h>

#include "models/NetworkState.h"

class GatewayWiFiManager {
 public:
  void begin(uint32_t now);
  void update(NetworkState& state, uint32_t now);

 private:
  void startConnection(uint32_t now);
  void scheduleReconnect(uint32_t now);

  bool connecting_ = false;
  bool wasConnected_ = false;
  uint8_t reconnectAttempt_ = 0;
  uint32_t attemptStartedMs_ = 0;
  uint32_t nextReconnectMs_ = 0;
};
