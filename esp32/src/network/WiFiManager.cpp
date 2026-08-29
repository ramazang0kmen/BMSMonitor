#include "network/WiFiManager.h"

#include <WiFi.h>

#include "config/NetworkConfig.h"
#include "config/Secrets.h"

void GatewayWiFiManager::begin(uint32_t now) {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(false);
  startConnection(now);
}

void GatewayWiFiManager::update(NetworkState& state, uint32_t now) {
  const bool connected = WiFi.status() == WL_CONNECTED;
  state.wifiConnected = connected;
  if (connected) {
    state.wifiRssi = WiFi.RSSI();
    state.ip = WiFi.localIP().toString();
    connecting_ = false;
    reconnectAttempt_ = 0;
    if (!wasConnected_) {
      Serial.printf("[WIFI] Connected | IP %s | RSSI %d dBm\n", state.ip.c_str(), state.wifiRssi);
    }
    wasConnected_ = true;
    return;
  }

  state.wifiRssi = 0;
  state.ip = "";
  if (wasConnected_) {
    Serial.println("[WIFI] Disconnected; local BLE/TFT monitoring continues");
    wasConnected_ = false;
    connecting_ = false;
    scheduleReconnect(now);
  }
  if (connecting_ && now - attemptStartedMs_ >= 12000U) {
    connecting_ = false;
    scheduleReconnect(now);
  }
  if (!connecting_ && static_cast<int32_t>(now - nextReconnectMs_) >= 0) startConnection(now);
}

void GatewayWiFiManager::startConnection(uint32_t now) {
  Serial.printf("[WIFI] Connecting to %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  connecting_ = true;
  attemptStartedMs_ = now;
}

void GatewayWiFiManager::scheduleReconnect(uint32_t now) {
  const uint8_t last = sizeof(NetworkConfig::WIFI_RECONNECT_BACKOFF_MS) /
                       sizeof(NetworkConfig::WIFI_RECONNECT_BACKOFF_MS[0]) - 1;
  const uint8_t slot = reconnectAttempt_ > last ? last : reconnectAttempt_;
  const uint32_t waitMs = NetworkConfig::WIFI_RECONNECT_BACKOFF_MS[slot];
  nextReconnectMs_ = now + waitMs;
  if (reconnectAttempt_ < last) ++reconnectAttempt_;
  Serial.printf("[WIFI] Reconnect in %lu ms\n", static_cast<unsigned long>(waitMs));
}
