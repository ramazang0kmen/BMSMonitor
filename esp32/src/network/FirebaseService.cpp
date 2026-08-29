#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include "network/FirebaseService.h"

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>

#include "config/BmsConfig.h"
#include "config/NetworkConfig.h"
#include "config/Secrets.h"

namespace {
String deviceRootPath() { return String("/devices/") + DEVICE_ID; }

bool isBmsOnline(const BmsDeviceState& state, uint32_t now) {
  return state.connected && state.lastUpdateMs != 0 &&
         now - state.lastUpdateMs <= BmsConfig::DATA_STALE_MS;
}

void appendBmsJson(String& json, const BmsDeviceState& state, bool online) {
  const BatteryData& data = state.data;
  json += "{\"online\":";
  json += online ? "true" : "false";
  json += ",\"name\":\"";
  json += state.name;
  json += "\",\"rssi\":";
  json += state.rssi;
  json += ",\"voltage\":";
  json += String(data.voltage, 2);
  json += ",\"current\":";
  json += String(data.current, 2);
  json += ",\"power\":";
  json += String(data.power, 1);
  json += ",\"soc\":";
  json += data.soc;
  json += ",\"remainingAh\":";
  json += String(data.remainingCapacityAh, 2);
  json += ",\"capacityAh\":";
  json += String(data.nominalCapacityAh, 2);
  for (uint8_t i = 0; i < 3; ++i) {
    json += ",\"temperature";
    json += i + 1;
    json += "\":";
    json += String(i < data.temperatureCount ? data.temperatures[i] : 0.0f, 1);
  }
  json += ",\"minCellVoltage\":";
  json += String(data.minCellVoltage, 3);
  json += ",\"maxCellVoltage\":";
  json += String(data.maxCellVoltage, 3);
  json += ",\"cellDeltaMv\":";
  json += static_cast<int>(lroundf(data.cellDelta * 1000.0f));
  json += ",\"chargeMos\":";
  json += data.chargeMos ? "true" : "false";
  json += ",\"dischargeMos\":";
  json += data.dischargeMos ? "true" : "false";
  json += ",\"balancing\":";
  json += data.balancing ? "true" : "false";
  json += ",\"protectionFlags\":";
  json += data.protectionFlags;
  json += ",\"cells\":[";
  for (uint8_t i = 0; i < data.cellCount; ++i) {
    if (i) json += ',';
    json += String(data.cellVoltages[i], 3);
  }
  json += "]}";
}
}  // namespace

struct FirebaseService::Impl {
  enum class Operation : uint8_t { None, SessionGet, StatusUpdate, LiveUpdate };

  WiFiClientSecure sslClient;
  AsyncClientClass asyncClient;
  UserAuth userAuth;
  FirebaseApp app;
  RealtimeDatabase database;
  AsyncResult authResult;
  AsyncResult operationResult;
  Operation operation = Operation::None;
  bool initialized = false;
  bool readyWasLogged = false;
  bool operationInFlight = false;
  uint32_t nextSessionCheckMs = 0;
  uint32_t nextStatusMs = 0;
  uint32_t lastTelemetryAttemptMs = 0;
  uint32_t uploadedBms1UpdateMs = 0;
  uint32_t uploadedBms2UpdateMs = 0;
  uint32_t pendingBms1UpdateMs = 0;
  uint32_t pendingBms2UpdateMs = 0;
  uint64_t lastHandledSessionRequest = 0;
  size_t livePayloadSize = 0;

  Impl()
      : asyncClient(sslClient),
        userAuth(FIREBASE_API_KEY, FIREBASE_DEVICE_EMAIL, FIREBASE_DEVICE_PASSWORD, 3000) {}

  void begin() {
    sslClient.setInsecure();
    sslClient.setHandshakeTimeout(5);
  }

  void initialize() {
    if (initialized) return;
    Serial.println("[FIREBASE] Authenticating...");
    initializeApp(asyncClient, app, getAuth(userAuth), authResult);
    app.getApp<RealtimeDatabase>(database);
    database.url(FIREBASE_DATABASE_URL);
    initialized = true;
  }

  void processAuthResult() {
    if (!authResult.isResult()) return;
    if (authResult.isError() && NetworkConfig::DEBUG_FIREBASE) {
      Serial.printf("[FIREBASE] Auth error: %s (%d)\n",
                    authResult.error().message().c_str(), authResult.error().code());
    }
  }

  void processOperation(NetworkState& network, uint64_t epochMs) {
    if (!operationInFlight || !operationResult.isResult()) return;
    if (operationResult.isError()) {
      Serial.printf("[FIREBASE] Operation failed: %s (%d)\n",
                    operationResult.error().message().c_str(), operationResult.error().code());
      operationInFlight = false;
      operation = Operation::None;
      return;
    }
    if (!operationResult.available()) return;

    if (operation == Operation::SessionGet) {
      network.remoteSessionRequestedAt =
          strtoull(operationResult.c_str(), nullptr, 10);
    } else if (operation == Operation::StatusUpdate) {
      network.lastStatusUpload = epochMs;
      if (NetworkConfig::DEBUG_FIREBASE) Serial.println("[FIREBASE] Status heartbeat uploaded");
    } else if (operation == Operation::LiveUpdate) {
      network.lastTelemetryUpload = epochMs;
      uploadedBms1UpdateMs = pendingBms1UpdateMs;
      uploadedBms2UpdateMs = pendingBms2UpdateMs;
      if (NetworkConfig::DEBUG_FIREBASE)
        Serial.printf("[FIREBASE] Live telemetry uploaded | %u bytes\n",
                      static_cast<unsigned>(livePayloadSize));
    }
    operationInFlight = false;
    operation = Operation::None;
  }

  void updateRemoteSessionState(NetworkState& network, uint64_t epochMs) {
    const uint64_t requestedAt = network.remoteSessionRequestedAt;
    const bool newRequest = requestedAt > 0 && requestedAt != lastHandledSessionRequest;
    if (newRequest) {
      lastHandledSessionRequest = requestedAt;
      network.remoteSessionExpiresAt =
          requestedAt + NetworkConfig::REMOTE_SESSION_DURATION_MS;
    }

    const bool active = network.timeSynced && requestedAt > 0 &&
                        epochMs >= requestedAt &&
                        epochMs - requestedAt <
                            NetworkConfig::REMOTE_SESSION_DURATION_MS;

    if (newRequest && active) {
      Serial.println("[REMOTE] New session requested");
      Serial.println("[REMOTE] Session ACTIVE for 60 seconds");
    }

    if (active == network.remoteSessionActive) return;
    const bool wasActive = network.remoteSessionActive;
    network.remoteSessionActive = active;
    if (wasActive && !active) {
      Serial.println("[REMOTE] Session expired - live telemetry paused");
    }
  }

  void startSessionGet(uint32_t now) {
    nextSessionCheckMs = now + NetworkConfig::REMOTE_SESSION_CHECK_INTERVAL_MS;
    operationResult.clear();
    operation = Operation::SessionGet;
    operationInFlight = true;
    database.get(asyncClient,
                 deviceRootPath() + "/control/remoteSession/requestedAt",
                 operationResult, false);
  }

  void startStatusUpdate(NetworkState& network, const SystemBatteryData& system,
                         uint32_t now) {
    nextStatusMs = now + NetworkConfig::STATUS_HEARTBEAT_INTERVAL_MS;
    String json;
    json.reserve(320);
    json = "{\"online\":true,\"lastSeen\":{\".sv\":\"timestamp\"},";
    json += "\"firmwareVersion\":\"";
    json += NetworkConfig::FIRMWARE_VERSION;
    json += "\",\"wifiRssi\":";
    json += network.wifiRssi;
    json += ",\"ip\":\"";
    json += network.ip;
    json += "\",\"uptimeSeconds\":";
    json += now / 1000U;
    json += ",\"freeHeap\":";
    json += ESP.getFreeHeap();
    json += ",\"minFreeHeap\":";
    json += ESP.getMinFreeHeap();
    json += ",\"bms1Online\":";
    json += system.bms1Online ? "true" : "false";
    json += ",\"bms2Online\":";
    json += system.bms2Online ? "true" : "false";
    json += '}';
    operationResult.clear();
    operation = Operation::StatusUpdate;
    operationInFlight = true;
    database.update<object_t>(asyncClient, deviceRootPath() + "/status",
                              object_t(json.c_str()), operationResult);
  }

  void startLiveUpdate(const SystemBatteryData& system, const BmsDeviceState& bms1,
                       const BmsDeviceState& bms2, uint32_t now) {
    String json;
    json.reserve(1500);
    json = "{\"timestamp\":{\".sv\":\"timestamp\"},\"system\":{";
    json += "\"voltage\":" + String(system.voltage, 2);
    json += ",\"current\":" + String(system.current, 2);
    json += ",\"power\":" + String(system.power, 1);
    json += ",\"soc\":" + String(system.soc, 2);
    json += ",\"remainingAh\":" + String(system.remainingAh, 2);
    json += ",\"totalCapacityAh\":" + String(system.totalCapacityAh, 2);
    json += ",\"socMismatch\":";
    json += system.socMismatch ? "true" : "false";
    json += ",\"socDelta\":" + String(system.socDelta, 1);
    json += "},\"bms1\":";
    appendBmsJson(json, bms1, isBmsOnline(bms1, now));
    json += ",\"bms2\":";
    appendBmsJson(json, bms2, isBmsOnline(bms2, now));
    json += '}';
    livePayloadSize = json.length();
    pendingBms1UpdateMs = bms1.lastUpdateMs;
    pendingBms2UpdateMs = bms2.lastUpdateMs;
    lastTelemetryAttemptMs = now;
    operationResult.clear();
    operation = Operation::LiveUpdate;
    operationInFlight = true;
    database.update<object_t>(asyncClient, deviceRootPath() + "/live",
                              object_t(json.c_str()), operationResult);
  }
};

FirebaseService::FirebaseService() : impl_(new Impl()) {}
FirebaseService::~FirebaseService() { delete impl_; }
void FirebaseService::begin() { impl_->begin(); }

void FirebaseService::update(NetworkState& network, const SystemBatteryData& system,
                             const BmsDeviceState& bms1, const BmsDeviceState& bms2,
                             uint32_t now, uint64_t epochMs) {
  if (network.wifiConnected && !impl_->initialized) impl_->initialize();
  if (!impl_->initialized) return;

  impl_->app.loop();
  impl_->processAuthResult();
  const bool ready = network.wifiConnected && impl_->app.ready();
  network.firebaseReady = ready;
  if (ready && !impl_->readyWasLogged) {
    Serial.println("[FIREBASE] Authenticated");
    Serial.println("[FIREBASE] RTDB ready");
    impl_->readyWasLogged = true;
  } else if (!ready && impl_->readyWasLogged) {
    Serial.println("[FIREBASE] RTDB unavailable; local monitoring continues");
    impl_->readyWasLogged = false;
  }

  impl_->processOperation(network, epochMs);
  impl_->updateRemoteSessionState(network, epochMs);
  if (!ready || impl_->operationInFlight) return;

  if (network.timeSynced &&
      static_cast<int32_t>(now - impl_->nextSessionCheckMs) >= 0) {
    impl_->startSessionGet(now);
    return;
  }
  if (network.timeSynced &&
      static_cast<int32_t>(now - impl_->nextStatusMs) >= 0) {
    impl_->startStatusUpdate(network, system, now);
    return;
  }
  const bool freshSnapshot = bms1.lastUpdateMs != impl_->uploadedBms1UpdateMs ||
                             bms2.lastUpdateMs != impl_->uploadedBms2UpdateMs;
  if (network.remoteSessionActive && freshSnapshot &&
      now - impl_->lastTelemetryAttemptMs >= NetworkConfig::TELEMETRY_INTERVAL_MS) {
    impl_->startLiveUpdate(system, bms1, bms2, now);
  }
}

size_t FirebaseService::lastLivePayloadSize() const { return impl_->livePayloadSize; }
