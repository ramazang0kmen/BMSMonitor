#include "network/WiFiManager.h"

#include <WiFi.h>

#include "config/NetworkConfig.h"
#include "config/Secrets.h"

namespace {
struct WiFiCredential {
  const char* ssid;
  const char* password;
};

#if defined(WIFI_PRIMARY_SSID) && defined(WIFI_PRIMARY_PASSWORD)
constexpr WiFiCredential NETWORKS[] = {
    {WIFI_PRIMARY_SSID, WIFI_PRIMARY_PASSWORD},
    {WIFI_SSID, WIFI_PASSWORD},
};
#else
constexpr WiFiCredential NETWORKS[] = {{WIFI_SSID, WIFI_PASSWORD}};
#endif

constexpr uint8_t NETWORK_COUNT = sizeof(NETWORKS) / sizeof(NETWORKS[0]);

void logWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WIFI][EVENT] STA connected to access point");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[WIFI][EVENT] IP acquired: %s\n", WiFi.localIP().toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[WIFI][EVENT] STA disconnected | reason=%u\n",
                    static_cast<unsigned>(info.wifi_sta_disconnected.reason));
      break;
    default:
      break;
  }
}
}  // namespace

void GatewayWiFiManager::begin(uint32_t now) {
  WiFi.onEvent(logWiFiEvent);
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
    networkIndex_ = (networkIndex_ + 1U) % NETWORK_COUNT;
    scheduleReconnect(now);
  }
  if (!connecting_ && static_cast<int32_t>(now - nextReconnectMs_) >= 0) startConnection(now);
}

void GatewayWiFiManager::startConnection(uint32_t now) {
  const WiFiCredential& network = NETWORKS[networkIndex_];
  Serial.printf("[WIFI] Connecting to %s (%u/%u)\n", network.ssid,
                static_cast<unsigned>(networkIndex_ + 1U),
                static_cast<unsigned>(NETWORK_COUNT));
  WiFi.begin(network.ssid, network.password);
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
