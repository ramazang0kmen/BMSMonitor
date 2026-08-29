#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include "network/FirebaseService.h"

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>

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
  enum class HealthState : uint8_t { Starting, Healthy, Degraded, Recovering };

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
  bool queueDraining = false;
  HealthState healthState = HealthState::Starting;
  uint32_t operationStartedMs = 0;
  uint32_t lastUpdateCallMs = 0;
  uint32_t maxUpdateGapMs = 0;
  uint32_t nextHealthLogMs = 0;
  uint32_t firebaseReadySinceMs = 0;
  uint32_t nextRecoveryMs = 0;
  uint8_t recoveryAttempt = 0;
  bool controlStaleWarned = false;
  bool authWasReady = false;
  bool authRefreshInProgress = false;
  bool authRefreshWindowLogged = false;
  unsigned long previousAuthTtl = 0;
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
    if (authResult.isEvent()) {
      const int code = authResult.eventLog().code();
      Serial.printf("[AUTH] Event: %s (%d)\n",
                    authResult.eventLog().message().c_str(), code);
      if (code == auth_event_authenticating && authWasReady && !authRefreshInProgress) {
        authRefreshInProgress = true;
        Serial.printf("[AUTH] Token refresh started | uptime=%lu s\n",
                      static_cast<unsigned long>(millis() / 1000U));
      } else if (code == auth_event_ready && authRefreshInProgress) {
        authRefreshInProgress = false;
        authRefreshWindowLogged = false;
        Serial.printf("[AUTH] Token refreshed successfully | ttl=%lu s\n", app.ttl());
      }
    }
    if (authResult.isError()) {
      Serial.printf("[AUTH][ERROR] %s (%d)\n",
                    authResult.error().message().c_str(), authResult.error().code());
      if (authRefreshInProgress) {
        Serial.println("[AUTH][ERROR] Token refresh failed");
        authRefreshInProgress = false;
      }
    }
  }

  const char* operationName() const {
    switch (operation) {
      case Operation::SessionGet: return "control-read";
      case Operation::StatusUpdate: return "status-write";
      case Operation::LiveUpdate: return "live-write";
      default: return "idle";
    }
  }

  const char* healthName() const {
    switch (healthState) {
      case HealthState::Healthy: return "healthy";
      case HealthState::Degraded: return "degraded";
      case HealthState::Recovering: return "recovering";
      default: return "starting";
    }
  }

  void finishOperation() {
    operationInFlight = false;
    operation = Operation::None;
    operationStartedMs = 0;
  }

  void noteSuccess(NetworkState& network, uint32_t now, bool read) {
    if (read) network.lastFirebaseReadSuccessMs = now;
    else network.lastFirebaseWriteSuccessMs = now;
    if (healthState == HealthState::Degraded || healthState == HealthState::Recovering) {
      Serial.printf("[FIREBASE] Recovery complete after %u attempt(s)\n", recoveryAttempt);
    }
    healthState = HealthState::Healthy;
    recoveryAttempt = 0;
    nextRecoveryMs = 0;
  }

  void processOperation(NetworkState& network, uint32_t now, uint64_t epochMs) {
    if (!operationInFlight || !operationResult.isResult()) return;
    if (operationResult.isEvent() && NetworkConfig::DEBUG_FIREBASE_HEALTH_LOGS) {
      Serial.printf("[FIREBASE][EVENT] %s: %s (%d)\n", operationName(),
                    operationResult.eventLog().message().c_str(),
                    operationResult.eventLog().code());
    }
    if (operationResult.isError()) {
      Serial.printf("[FIREBASE][ERROR] %s failed after %lu ms: %s (%d)\n",
                    operationName(), static_cast<unsigned long>(now - operationStartedMs),
                    operationResult.error().message().c_str(), operationResult.error().code());
      finishOperation();
      return;
    }
    if (!operationResult.available()) return;

    if (operation == Operation::SessionGet) {
      network.remoteSessionRequestedAt =
          strtoull(operationResult.c_str(), nullptr, 10);
      network.lastControlReadSuccessMs = now;
      noteSuccess(network, now, true);
      controlStaleWarned = false;
    } else if (operation == Operation::StatusUpdate) {
      network.lastStatusUpload = epochMs;
      network.lastStatusWriteSuccessMs = now;
      noteSuccess(network, now, false);
      if (NetworkConfig::DEBUG_FIREBASE) Serial.println("[FIREBASE] Status heartbeat uploaded");
    } else if (operation == Operation::LiveUpdate) {
      network.lastTelemetryUpload = epochMs;
      network.lastLiveWriteSuccessMs = now;
      noteSuccess(network, now, false);
      uploadedBms1UpdateMs = pendingBms1UpdateMs;
      uploadedBms2UpdateMs = pendingBms2UpdateMs;
      if (NetworkConfig::DEBUG_FIREBASE)
        Serial.printf("[FIREBASE] Live telemetry uploaded | %u bytes\n",
                      static_cast<unsigned>(livePayloadSize));
    }
    finishOperation();
  }

  void cancelStuckOperation(uint32_t now, const char* reason) {
    if (operationInFlight) {
      Serial.printf("[FIREBASE][WARN] Cancelling %s after %lu ms (%s)\n",
                    operationName(), static_cast<unsigned long>(now - operationStartedMs), reason);
    } else {
      Serial.printf("[FIREBASE][WARN] Resetting async RTDB queue (%s)\n", reason);
    }
    asyncClient.stopAsync(true);
    operationResult.clear();
    finishOperation();
    queueDraining = true;
  }

  void serviceWatchdog(NetworkState& network, uint32_t now, bool ready) {
    if (operationInFlight && now - operationStartedMs >=
                                 NetworkConfig::FIREBASE_OPERATION_TIMEOUT_MS) {
      cancelStuckOperation(now, "operation deadline exceeded");
      healthState = HealthState::Recovering;
      const uint8_t last = sizeof(NetworkConfig::FIREBASE_RECOVERY_BACKOFF_MS) /
                           sizeof(NetworkConfig::FIREBASE_RECOVERY_BACKOFF_MS[0]) - 1;
      const uint8_t slot = recoveryAttempt > last ? last : recoveryAttempt;
      nextRecoveryMs = now + NetworkConfig::FIREBASE_RECOVERY_BACKOFF_MS[slot];
      if (recoveryAttempt < 255) ++recoveryAttempt;
      if (!app.isAuthenticated() || app.isExpired()) {
        Serial.println("[AUTH] Restarting authentication after timed-out RTDB task");
        app.authenticate();
      }
    }
    if (!ready || firebaseReadySinceMs == 0) return;

    const uint32_t readAge = now - network.lastFirebaseReadSuccessMs;
    const uint32_t writeAge = now - network.lastFirebaseWriteSuccessMs;
    if (readAge >= NetworkConfig::FIREBASE_CONTROL_STALE_WARN_MS && !controlStaleWarned) {
      Serial.printf("[FIREBASE][WARN] Control reads stale for %lu ms\n",
                    static_cast<unsigned long>(readAge));
      controlStaleWarned = true;
    }
    if (readAge < NetworkConfig::FIREBASE_RECOVERY_THRESHOLD_MS ||
        writeAge < NetworkConfig::FIREBASE_RECOVERY_THRESHOLD_MS ||
        static_cast<int32_t>(now - nextRecoveryMs) < 0) return;

    healthState = HealthState::Degraded;
    Serial.printf("[FIREBASE][DEGRADED] readAge=%lu ms writeAge=%lu ms tasks=%u\n",
                  static_cast<unsigned long>(readAge), static_cast<unsigned long>(writeAge),
                  static_cast<unsigned>(asyncClient.taskCount()));
    cancelStuckOperation(now, "read and write progress lost");
    if (!app.isAuthenticated() || app.isExpired()) {
      Serial.println("[AUTH] Forcing authentication refresh during recovery");
      app.authenticate();
    }
    healthState = HealthState::Recovering;
    const uint8_t last = sizeof(NetworkConfig::FIREBASE_RECOVERY_BACKOFF_MS) /
                         sizeof(NetworkConfig::FIREBASE_RECOVERY_BACKOFF_MS[0]) - 1;
    const uint8_t slot = recoveryAttempt > last ? last : recoveryAttempt;
    const uint32_t backoff = NetworkConfig::FIREBASE_RECOVERY_BACKOFF_MS[slot];
    nextRecoveryMs = now + backoff;
    if (recoveryAttempt < 255) ++recoveryAttempt;
    Serial.printf("[FIREBASE] Recovery attempt %u; retry in %lu ms\n", recoveryAttempt,
                  static_cast<unsigned long>(backoff));
  }

  void serviceQueueDrain() {
    if (!queueDraining || asyncClient.taskCount() != 0) return;
    queueDraining = false;
    Serial.println("[FIREBASE] Async RTDB queue drained");
  }

  void trackAuthLifecycle(bool ready) {
    const bool authenticated = app.isAuthenticated();
    const unsigned long ttl = app.ttl();
    if (authenticated && !authWasReady) {
      Serial.printf("[AUTH] Token ready | ttl=%lu s\n", ttl);
    } else if (!authenticated && authWasReady) {
      Serial.println("[AUTH][WARN] Token no longer authenticated");
    }
    if (ready && authenticated && previousAuthTtl > 0 && ttl > previousAuthTtl + 60) {
      Serial.printf("[AUTH] Token refreshed successfully | ttl=%lu s\n", ttl);
      authRefreshWindowLogged = false;
    }
    if (authenticated && ttl <= 300 && !authRefreshWindowLogged) {
      Serial.printf("[AUTH] Token refresh window entered | ttl=%lu s\n", ttl);
      authRefreshWindowLogged = true;
    }
    authWasReady = authenticated;
    previousAuthTtl = ttl;
  }

  void printHealth(const NetworkState& network, uint32_t now, uint64_t epochMs) {
    if (!NetworkConfig::DEBUG_FIREBASE_HEALTH_LOGS ||
        static_cast<int32_t>(now - nextHealthLogMs) < 0) return;
    nextHealthLogMs = now + NetworkConfig::FIREBASE_HEALTH_LOG_INTERVAL_MS;
    const uint32_t readAge = network.lastFirebaseReadSuccessMs == 0
                                 ? 0 : now - network.lastFirebaseReadSuccessMs;
    const uint32_t writeAge = network.lastFirebaseWriteSuccessMs == 0
                                  ? 0 : now - network.lastFirebaseWriteSuccessMs;
    const bool internetReachable = network.firebaseReady &&
        (network.lastFirebaseReadSuccessMs != 0 || network.lastFirebaseWriteSuccessMs != 0) &&
        (readAge < NetworkConfig::FIREBASE_RECOVERY_THRESHOLD_MS ||
         writeAge < NetworkConfig::FIREBASE_RECOVERY_THRESHOLD_MS);
    Serial.printf(
        "[HEALTH] up=%lus wifi=%d ip=%s rssi=%d internet=%d time=%d epoch=%llu "
        "app=%d auth=%d rtdb=%d ttl=%lus state=%s op=%s/%d drain=%d tasks=%u gapMax=%lums "
        "readAge=%lums writeAge=%lums heap=%u minHeap=%u largest=%u remote=%d req=%llu\n",
        static_cast<unsigned long>(now / 1000U), network.wifiConnected,
        network.ip.c_str(), network.wifiRssi, internetReachable, network.timeSynced,
        static_cast<unsigned long long>(epochMs), app.ready(), app.isAuthenticated(),
        network.firebaseReady, app.ttl(), healthName(), operationName(), operationInFlight,
        queueDraining,
        static_cast<unsigned>(asyncClient.taskCount()), static_cast<unsigned long>(maxUpdateGapMs),
        static_cast<unsigned long>(readAge), static_cast<unsigned long>(writeAge),
        ESP.getFreeHeap(), ESP.getMinFreeHeap(),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), network.remoteSessionActive,
        static_cast<unsigned long long>(network.remoteSessionRequestedAt));
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
      Serial.printf("[REMOTE] requestedAt=%llu age=%llu ms\n",
                    static_cast<unsigned long long>(requestedAt),
                    static_cast<unsigned long long>(epochMs - requestedAt));
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
    operationStartedMs = now;
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
    operationStartedMs = now;
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
    operationStartedMs = now;
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
  if (impl_->lastUpdateCallMs != 0) {
    const uint32_t gap = now - impl_->lastUpdateCallMs;
    if (gap > impl_->maxUpdateGapMs) impl_->maxUpdateGapMs = gap;
  }
  impl_->lastUpdateCallMs = now;
  if (network.wifiConnected && !impl_->initialized) impl_->initialize();
  if (!impl_->initialized) return;

  impl_->app.loop();
  impl_->serviceQueueDrain();
  impl_->processAuthResult();
  const bool ready = network.wifiConnected && impl_->app.ready();
  network.firebaseReady = ready;
  impl_->trackAuthLifecycle(ready);
  if (ready && !impl_->readyWasLogged) {
    Serial.println("[FIREBASE] Authenticated");
    Serial.println("[FIREBASE] RTDB ready");
    impl_->readyWasLogged = true;
    impl_->firebaseReadySinceMs = now;
    network.lastFirebaseReadSuccessMs = now;
    network.lastFirebaseWriteSuccessMs = now;
  } else if (!ready && impl_->readyWasLogged) {
    Serial.println("[FIREBASE] RTDB unavailable; local monitoring continues");
    impl_->readyWasLogged = false;
    impl_->firebaseReadySinceMs = 0;
  }

  impl_->processOperation(network, now, epochMs);
  impl_->updateRemoteSessionState(network, epochMs);
  impl_->serviceWatchdog(network, now, ready);
  impl_->printHealth(network, now, epochMs);
  if (!ready || impl_->operationInFlight || impl_->queueDraining ||
      static_cast<int32_t>(now - impl_->nextRecoveryMs) < 0) return;

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
