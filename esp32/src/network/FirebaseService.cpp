#define ENABLE_USER_AUTH
#define ENABLE_DATABASE
#define ENABLE_FIRESTORE

#include "network/FirebaseService.h"

#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <time.h>

#include "config/BmsConfig.h"
#include "config/LogConfig.h"
#include "config/NetworkConfig.h"
#include "config/Secrets.h"
#include "monitoring/EventLogMonitor.h"

#ifndef FIREBASE_PROJECT_ID
#error "Define FIREBASE_PROJECT_ID in include/config/Secrets.h (see Secrets.example.h)"
#endif

namespace {
String deviceRootPath() { return String("/devices/") + DEVICE_ID; }

// millis()-domain "which timestamp is more recent", wrap-safe, treating 0 as
// "never".
uint32_t laterOf(uint32_t a, uint32_t b) {
  if (a == 0) return b;
  if (b == 0) return a;
  return static_cast<int32_t>(a - b) >= 0 ? a : b;
}

bool isBmsOnline(const BmsDeviceState& state, uint32_t now) {
  return state.hasFreshData(now);
}

void appendBmsJson(String& json, const BmsDeviceState& state, bool online,
                   uint32_t now, uint64_t epochMs) {
  const BatteryData& data = state.data;
  json += "{\"online\":";
  json += online ? "true" : "false";
  json += ",\"connected\":";
  json += state.connected ? "true" : "false";
  json += ",\"id\":\"";
  json += state.id;
  json += "\",\"order\":";
  json += state.order;
  json += ",\"name\":\"";
  json += state.name;
  json += "\",\"lastSeen\":";
  const uint64_t lastSeen = state.lastUpdateMs == 0 || now < state.lastUpdateMs
                                ? 0 : epochMs - (now - state.lastUpdateMs);
  char lastSeenText[24];
  snprintf(lastSeenText, sizeof(lastSeenText), "%llu",
           static_cast<unsigned long long>(lastSeen));
  json += lastSeenText;
  json += ",\"rssi\":";
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

Values::Value numberValue(float value, int decimals) {
  return Values::Value(Values::DoubleValue(number_t(value, decimals)));
}

Values::Value integerValue(int64_t value) { return Values::Value(Values::IntegerValue(value)); }

Values::Value boolValue(bool value) { return Values::Value(Values::BooleanValue(value)); }

Values::Value textValue(const char* value) { return Values::Value(Values::StringValue(value)); }

Values::Value systemValue(const SystemSnapshot& system) {
  Values::MapValue map("voltage", numberValue(system.voltage, 2));
  map.add("current", numberValue(system.current, 2));
  map.add("power", numberValue(system.power, 1));
  map.add("soc", numberValue(system.soc, 2));
  map.add("remainingAh", numberValue(system.remainingAh, 2));
  map.add("socDelta", numberValue(system.socDelta, 1));
  return Values::Value(map);
}

Values::Value bmsValue(const BmsSnapshot& bms) {
  Values::MapValue map("online", boolValue(bms.online));
  map.add("soc", integerValue(bms.soc));
  map.add("voltage", numberValue(bms.voltage, 2));
  map.add("current", numberValue(bms.current, 2));
  map.add("power", numberValue(bms.power, 1));
  map.add("remainingAh", numberValue(bms.remainingAh, 2));
  map.add("minCellVoltage", numberValue(bms.minCellVoltage, 3));
  map.add("maxCellVoltage", numberValue(bms.maxCellVoltage, 3));
  map.add("temperature", numberValue(bms.temperature, 1));
  map.add("chargeMos", boolValue(bms.chargeMos));
  map.add("dischargeMos", boolValue(bms.dischargeMos));
  map.add("protectionFlags", integerValue(bms.protectionFlags));
  return Values::Value(map);
}

String isoTimestamp(uint64_t epochMs) {
  const time_t seconds = static_cast<time_t>(epochMs / 1000ULL);
  tm utc{};
  gmtime_r(&seconds, &utc);
  char text[32];
  snprintf(text, sizeof(text), "%04d-%02d-%02dT%02d:%02d:%02d.%03uZ", utc.tm_year + 1900,
           utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec,
           static_cast<unsigned>(epochMs % 1000ULL));
  return String(text);
}
}  // namespace

struct FirebaseService::Impl {
  enum class Operation : uint8_t { None, SessionGet, StatusUpdate, LiveUpdate, LogWrite };
  enum class HealthState : uint8_t { Starting, Healthy, Degraded, Recovering };

  WiFiClientSecure sslClient;
  AsyncClientClass asyncClient;
  UserAuth userAuth;
  FirebaseApp app;
  RealtimeDatabase database;
  Firestore::Documents firestore;
  AsyncResult authResult;
  AsyncResult operationResult;
  Operation operation = Operation::None;
  bool initialized = false;
  bool readyWasLogged = false;
  bool operationInFlight = false;
  bool queueDraining = false;
  // Recovery scheduled a retry (backoff armed) but it has not been issued yet.
  // While this is set, serviceWatchdog must not re-evaluate staleness and
  // re-arm the backoff - otherwise the retry it is waiting for never runs.
  bool recoveryRetryPending = false;
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
  uint32_t remoteSessionActivatedMs = 0;
  uint32_t uploadedDataRevision = 0;
  uint32_t pendingDataRevision = 0;
  uint64_t lastHandledSessionRequest = 0;
  // Set on every FirebaseService::update(); the queue outlives this object.
  EventLogMonitor* eventLog = nullptr;
  uint32_t nextLogWriteMs = 0;
  uint32_t logWriteFailCount = 0;
  size_t livePayloadSize = 0;

  // --- Diagnostics / per-channel health (added in 0.4.2-stability) ---
  // asyncClient.stopAsync() only *marks* a queued/running task for removal;
  // it does not touch the underlying socket (see cancelStuckOperation()).
  // queueDrainStartedMs bounds how long we wait for taskCount() to reach 0
  // before we stop trusting the library to clean up after itself.
  uint32_t queueDrainStartedMs = 0;
  uint32_t lastAuthSuccessMs = 0;
  uint32_t controlReadFailCount = 0;
  uint32_t statusWriteFailCount = 0;
  uint32_t liveWriteFailCount = 0;
  uint32_t firebaseErrorCount = 0;
  uint32_t totalRecoveryCount = 0;
  uint32_t forcedSocketCloseCount = 0;
  int lastErrorCode = 0;
  String lastErrorMessage;

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
    app.getApp<Firestore::Documents>(firestore);
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
        lastAuthSuccessMs = millis();
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
      case Operation::LogWrite: return "log-write";
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
    recoveryRetryPending = false;
  }

  void processOperation(NetworkState& network, uint32_t now, uint64_t epochMs) {
    if (!operationInFlight || !operationResult.isResult()) return;
    if (operationResult.isEvent() && NetworkConfig::DEBUG_FIREBASE_HEALTH_LOGS) {
      Serial.printf("[FIREBASE][EVENT] %s: %s (%d)\n", operationName(),
                    operationResult.eventLog().message().c_str(),
                    operationResult.eventLog().code());
    }
    if (operationResult.isError()) {
      ++firebaseErrorCount;
      lastErrorCode = operationResult.error().code();
      lastErrorMessage = operationResult.error().message();
      if (operation == Operation::SessionGet) ++controlReadFailCount;
      else if (operation == Operation::StatusUpdate) ++statusWriteFailCount;
      else if (operation == Operation::LiveUpdate) ++liveWriteFailCount;
      else if (operation == Operation::LogWrite) ++logWriteFailCount;
      Serial.printf("[FIREBASE][ERROR] %s failed after %lu ms: %s (%d) | retry=%u\n",
                    operationName(), static_cast<unsigned long>(now - operationStartedMs),
                    lastErrorMessage.c_str(), lastErrorCode,
                    static_cast<unsigned>(recoveryAttempt));
      if (operation == Operation::LogWrite) handleLogWriteError(now, lastErrorCode);
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
      uploadedDataRevision = pendingDataRevision;
      if (NetworkConfig::DEBUG_FIREBASE)
        Serial.printf("[FIREBASE] Live telemetry uploaded | %u bytes\n",
                      static_cast<unsigned>(livePayloadSize));
    } else if (operation == Operation::LogWrite) {
      noteSuccess(network, now, false);
      if (const LogEvent* event = eventLog->peek()) {
        Serial.printf("[LOG] Uploaded %s (%u left)\n", logEventTypeName(event->type),
                      static_cast<unsigned>(eventLog->pending() - 1));
      }
      eventLog->pop();
    }
    finishOperation();
  }

  // Cancel the in-flight (or last) RTDB task through the library's own API
  // only. asyncClient.stopAsync(true) flags the slot for removal; the library
  // frees it and re-opens a fresh TCP+TLS session by itself on the next
  // request (conn_handler::connect() calls client->stop() then client->connect()
  // internally). We must NOT call sslClient.stop() here: closing the socket
  // behind FirebaseClient's back leaves its connection state machine
  // inconsistent and every subsequent operation hangs forever with no
  // callback and no error (reproduced on hardware - Firebase went permanently
  // silent ~2 minutes after the first such cancel, 20+ recovery attempts,
  // zero successful operations).
  void cancelStuckOperation(uint32_t now, const char* reason) {
    if (operationInFlight) {
      Serial.printf("[FIREBASE][WARN] Cancelling %s after %lu ms (%s)\n",
                    operationName(), static_cast<unsigned long>(now - operationStartedMs), reason);
    } else {
      Serial.printf("[FIREBASE][WARN] Resetting async RTDB queue (%s)\n", reason);
    }
    // A timed-out log write counts as a failed attempt too, or its retry
    // backoff would be skipped and it would keep jumping the queue ahead of
    // the status heartbeat.
    if (operationInFlight && operation == Operation::LogWrite) {
      ++logWriteFailCount;
      handleLogWriteError(now, 0);
    }
    asyncClient.stopAsync(true);
    operationResult.clear();
    finishOperation();
    queueDraining = true;
    queueDrainStartedMs = now;
  }

  void enterRecovery(uint32_t now) {
    healthState = HealthState::Recovering;
    recoveryRetryPending = true;
    ++totalRecoveryCount;
    const uint8_t last = sizeof(NetworkConfig::FIREBASE_RECOVERY_BACKOFF_MS) /
                         sizeof(NetworkConfig::FIREBASE_RECOVERY_BACKOFF_MS[0]) - 1;
    const uint8_t slot = recoveryAttempt > last ? last : recoveryAttempt;
    const uint32_t backoff = NetworkConfig::FIREBASE_RECOVERY_BACKOFF_MS[slot];
    nextRecoveryMs = now + backoff;
    if (recoveryAttempt < 255) ++recoveryAttempt;
    Serial.printf("[FIREBASE] Recovery attempt %u (total %u); retry in %lu ms\n", recoveryAttempt,
                  totalRecoveryCount, static_cast<unsigned long>(backoff));
  }

  void serviceWatchdog(NetworkState& network, uint32_t now, bool ready) {
    if (operationInFlight && now - operationStartedMs >=
                                 NetworkConfig::FIREBASE_OPERATION_TIMEOUT_MS) {
      cancelStuckOperation(now, "operation deadline exceeded");
      enterRecovery(now);
      if (!app.isAuthenticated() || app.isExpired()) {
        Serial.println("[AUTH] Restarting authentication after timed-out RTDB task");
        app.authenticate();
      }
    }
    if (!ready || firebaseReadySinceMs == 0) return;

    // Per-channel ages are computed for DIAGNOSTICS ONLY (the DEGRADED line and
    // the [HEALTH] scenario field). They are NOT individual recovery triggers:
    // control-read, status-write and live-write all run over the same
    // asyncClient/TLS socket, so a genuine transport failure stops every
    // channel at once, whereas a single channel's age advancing on its own is
    // routinely benign - an idle viewer, a heartbeat that is merely due, or a
    // stale cross-session live baseline. Three earlier revisions each shipped a
    // different false-positive from per-channel triggers (heartbeat interval ==
    // threshold; live baseline 0; live baseline from a previous session), every
    // one of them a self-perpetuating DEGRADED<->recovery flap. The real
    // trigger below is "no channel has made progress at all".
    const uint32_t controlBaseline =
        network.lastControlReadSuccessMs == 0 ? firebaseReadySinceMs : network.lastControlReadSuccessMs;
    const uint32_t statusBaseline =
        network.lastStatusWriteSuccessMs == 0 ? firebaseReadySinceMs : network.lastStatusWriteSuccessMs;
    const uint32_t controlAge = now - controlBaseline;
    const uint32_t statusAge = now - statusBaseline;
    const bool controlStale = controlAge >= NetworkConfig::FIREBASE_CONTROL_STALE_MS;
    const bool statusStale = statusAge >= NetworkConfig::FIREBASE_STATUS_STALE_MS;

    bool liveStale = false;
    uint32_t liveAge = 0;
    if (network.remoteSessionActive) {
      // Most recent of {successful live write, live-write attempt, session
      // start}. Using laterOf() means a live write from a session that already
      // ended cannot serve as the baseline for a new one, and a baseline can
      // never read as being in the future relative to `now`.
      const uint32_t liveBaseline =
          laterOf(laterOf(network.lastLiveWriteSuccessMs, lastTelemetryAttemptMs),
                  remoteSessionActivatedMs);
      if (liveBaseline != 0 && static_cast<int32_t>(now - liveBaseline) > 0) {
        liveAge = now - liveBaseline;
        liveStale = liveAge >= NetworkConfig::FIREBASE_LIVE_STALE_MS;
      }
    }

    if (controlAge >= NetworkConfig::FIREBASE_CONTROL_STALE_WARN_MS && !controlStaleWarned) {
      Serial.printf("[FIREBASE][WARN] Control reads stale for %lu ms\n",
                    static_cast<unsigned long>(controlAge));
      controlStaleWarned = true;
    }

    // Recovery already armed a backoff and is waiting to issue its retry. Do
    // not re-evaluate here: an earlier revision re-ran cancel + re-armed the
    // backoff on the very loop pass that would otherwise have issued the
    // retry, so no operation was ever attempted again and the ages grew
    // without bound (Firebase silent forever after one trip).
    if (recoveryRetryPending) return;

    // THE recovery trigger: nothing - not control-read, not status-write, not
    // live-write - has completed for RECOVERY_THRESHOLD. A hung individual
    // request is already handled by the 10 s operation deadline at the top of
    // this function; this catches a transport that has quietly stopped
    // delivering results on every channel.
    const uint32_t lastAnyProgress = laterOf(
        laterOf(network.lastControlReadSuccessMs, network.lastStatusWriteSuccessMs),
        laterOf(network.lastLiveWriteSuccessMs, firebaseReadySinceMs));
    const uint32_t noProgressMs =
        lastAnyProgress != 0 && static_cast<int32_t>(now - lastAnyProgress) > 0
            ? now - lastAnyProgress
            : 0;
    if (noProgressMs < NetworkConfig::FIREBASE_RECOVERY_THRESHOLD_MS ||
        static_cast<int32_t>(now - nextRecoveryMs) < 0) return;

    // A request is genuinely in flight and still within its own 10 s deadline
    // (an overrun is handled at the top of this function). Leave it alone -
    // do not cancel a young request because a different channel's age crossed
    // the line.
    if (operationInFlight) {
      healthState = HealthState::Degraded;
      return;
    }

    healthState = HealthState::Degraded;
    Serial.printf(
        "[FIREBASE][DEGRADED] noProgress=%lums controlAge=%lums(stale=%d) "
        "statusAge=%lums(stale=%d) liveAge=%lums(stale=%d,active=%d) tasks=%u\n",
        static_cast<unsigned long>(noProgressMs),
        static_cast<unsigned long>(controlAge), controlStale,
        static_cast<unsigned long>(statusAge), statusStale,
        static_cast<unsigned long>(liveAge), liveStale, network.remoteSessionActive,
        static_cast<unsigned>(asyncClient.taskCount()));
    cancelStuckOperation(now, "no RTDB progress on any channel");
    if (!app.isAuthenticated() || app.isExpired()) {
      Serial.println("[AUTH] Forcing authentication refresh during recovery");
      app.authenticate();
    }
    enterRecovery(now);
  }

  // queueDraining is a bounded gate: FirebaseService::update() will not start a
  // new control-read/status-write/live-write while it is set. taskCount()
  // normally reaches 0 within a loop pass or two of stopAsync(true). Past the
  // deadline we simply stop gating on it and let the library rebuild its own
  // transport on the next request - we never close its socket ourselves (that
  // corrupts FirebaseClient's connection state; see cancelStuckOperation).
  void serviceQueueDrain(uint32_t now) {
    if (!queueDraining) return;
    if (asyncClient.taskCount() == 0) {
      queueDraining = false;
      Serial.println("[FIREBASE] Async RTDB queue drained");
      return;
    }
    if (now - queueDrainStartedMs >= NetworkConfig::FIREBASE_QUEUE_DRAIN_TIMEOUT_MS) {
      ++forcedSocketCloseCount;
      Serial.printf(
          "[FIREBASE][WARN] Queue drain deadline exceeded (tasks=%u); continuing without it\n",
          static_cast<unsigned>(asyncClient.taskCount()));
      asyncClient.stopAsync(true);
      queueDraining = false;
    }
  }

  void trackAuthLifecycle(bool ready, uint32_t now) {
    const bool authenticated = app.isAuthenticated();
    const unsigned long ttl = app.ttl();
    if (authenticated && !authWasReady) {
      Serial.printf("[AUTH] Token ready | ttl=%lu s\n", ttl);
      lastAuthSuccessMs = now;
    } else if (!authenticated && authWasReady) {
      Serial.println("[AUTH][WARN] Token no longer authenticated");
    }
    if (ready && authenticated && previousAuthTtl > 0 && ttl > previousAuthTtl + 60) {
      Serial.printf("[AUTH] Token refreshed successfully | ttl=%lu s\n", ttl);
      authRefreshWindowLogged = false;
      lastAuthSuccessMs = now;
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
    const uint32_t controlAge = network.lastControlReadSuccessMs == 0
                                    ? 0 : now - network.lastControlReadSuccessMs;
    const uint32_t statusAge = network.lastStatusWriteSuccessMs == 0
                                    ? 0 : now - network.lastStatusWriteSuccessMs;
    const uint32_t liveAge = network.lastLiveWriteSuccessMs == 0
                                  ? 0 : now - network.lastLiveWriteSuccessMs;
    const uint32_t authAge = lastAuthSuccessMs == 0 ? 0 : now - lastAuthSuccessMs;
    const bool internetReachable = network.firebaseReady &&
        (network.lastControlReadSuccessMs != 0 || network.lastStatusWriteSuccessMs != 0) &&
        (controlAge < NetworkConfig::FIREBASE_RECOVERY_THRESHOLD_MS ||
         statusAge < NetworkConfig::FIREBASE_RECOVERY_THRESHOLD_MS);
    // Scenario classification, per the spec's Senaryo A-E table.
    const bool controlDead = controlAge >= NetworkConfig::FIREBASE_CONTROL_STALE_MS;
    const bool statusDead = statusAge >= NetworkConfig::FIREBASE_STATUS_STALE_MS;
    const char* scenario = "OK";
    if (!network.wifiConnected) scenario = "D:wifi-dead";
    else if (!app.isAuthenticated()) scenario = "E:auth-dead";
    else if (controlDead && statusDead) scenario = "A:firebase-dead";
    else if (controlDead) scenario = "B:control-dead";
    else if (statusDead) scenario = "C:write-dead";
    Serial.printf(
        "[HEALTH] up=%lus wifi=%d ip=%s rssi=%d internet=%d time=%d epoch=%llu "
        "app=%d auth=%d rtdb=%d ttl=%lus authAge=%lums state=%s scenario=%s op=%s/%d drain=%d "
        "tasks=%u gapMax=%lums controlAge=%lums statusAge=%lums liveAge=%lums heap=%u minHeap=%u "
        "largest=%u remote=%d req=%llu recoveryCount=%u forcedClose=%u errCount=%u "
        "ctrlFail=%u statusFail=%u liveFail=%u logFail=%u lastErr=\"%s\"(%d)\n",
        static_cast<unsigned long>(now / 1000U), network.wifiConnected,
        network.ip.c_str(), network.wifiRssi, internetReachable, network.timeSynced,
        static_cast<unsigned long long>(epochMs), app.ready(), app.isAuthenticated(),
        network.firebaseReady, app.ttl(), static_cast<unsigned long>(authAge / 1000U),
        healthName(), scenario, operationName(), operationInFlight, queueDraining,
        static_cast<unsigned>(asyncClient.taskCount()), static_cast<unsigned long>(maxUpdateGapMs),
        static_cast<unsigned long>(controlAge), static_cast<unsigned long>(statusAge),
        static_cast<unsigned long>(liveAge),
        ESP.getFreeHeap(), ESP.getMinFreeHeap(),
        heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), network.remoteSessionActive,
        static_cast<unsigned long long>(network.remoteSessionRequestedAt), totalRecoveryCount,
        forcedSocketCloseCount, firebaseErrorCount, controlReadFailCount, statusWriteFailCount,
        liveWriteFailCount, logWriteFailCount, lastErrorMessage.c_str(), lastErrorCode);
  }

  void updateRemoteSessionState(NetworkState& network, uint64_t epochMs, uint32_t now) {
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
    if (!wasActive && active) {
      // Rising edge: give the live channel a staleness baseline so it is not
      // judged against 0 before its first write (see serviceWatchdog()). Use
      // the loop's `now`, not a fresh millis(), so it can never read as being
      // in the future relative to serviceWatchdog() on this same pass.
      remoteSessionActivatedMs = now;
    } else if (wasActive && !active) {
      Serial.println("[REMOTE] Session expired - live telemetry paused");
    }
  }

  // Document IDs are derived from the event itself, so a retry after a lost
  // response hits ALREADY_EXISTS (409) instead of writing a duplicate.
  void handleLogWriteError(uint32_t now, int httpCode) {
    LogEvent* event = eventLog ? eventLog->peek() : nullptr;
    if (!event) return;
    if (httpCode == 409) {
      Serial.printf("[LOG] %s already stored\n", logEventTypeName(event->type));
      eventLog->pop();
      return;
    }
    if (event->attempts < 255) ++event->attempts;
    if (event->attempts >= LogConfig::MAX_WRITE_ATTEMPTS) {
      Serial.printf("[LOG][ERROR] Giving up on %s after %u attempts\n",
                    logEventTypeName(event->type), static_cast<unsigned>(event->attempts));
      eventLog->pop();
      return;
    }
    constexpr size_t last = sizeof(LogConfig::WRITE_RETRY_BACKOFF_MS) /
                            sizeof(LogConfig::WRITE_RETRY_BACKOFF_MS[0]) - 1;
    const size_t slot = event->attempts - 1U > last ? last : event->attempts - 1U;
    nextLogWriteMs = now + LogConfig::WRITE_RETRY_BACKOFF_MS[slot];
  }

  void startLogWrite(const LogEvent& event, uint32_t now, uint64_t epochMs) {
    recoveryRetryPending = false;
    const uint64_t eventEpochMs = epochMs - (now - event.capturedMs);
    Document<Values::Value> doc("type", textValue(logEventTypeName(event.type)));
    doc.add("severity", textValue(logSeverityName(event.severity)));
    doc.add("createdAt", Values::Value(Values::TimestampValue(isoTimestamp(eventEpochMs))));
    doc.add("bmsIndex", integerValue(event.bmsIndex));
    doc.add("detail", integerValue(event.detail));
    doc.add("firmwareVersion", textValue(NetworkConfig::FIRMWARE_VERSION));
    doc.add("uptimeSeconds", integerValue(event.capturedMs / 1000U));
    doc.add("system", systemValue(event.system));
    doc.add("bms1", bmsValue(event.bms[0]));
    doc.add("bms2", bmsValue(event.bms[1]));
    if (event.hasBefore) {
      Values::MapValue before("voltage", numberValue(event.before.voltage, 2));
      before.add("current", numberValue(event.before.current, 2));
      before.add("power", numberValue(event.before.power, 1));
      before.add("soc", numberValue(event.before.soc, 2));
      before.add("bms1Voltage", numberValue(event.beforeBmsVoltage[0], 2));
      before.add("bms2Voltage", numberValue(event.beforeBmsVoltage[1], 2));
      doc.add("before", Values::Value(before));
    }

    String path = String("devices/") + DEVICE_ID + "/" + LogConfig::COLLECTION + "/";
    path += String(static_cast<unsigned long>(eventEpochMs / 1000ULL));
    path += '-';
    path += logEventTypeName(event.type);
    path += '-';
    path += event.bmsIndex;

    operationResult.clear();
    operation = Operation::LogWrite;
    operationInFlight = true;
    operationStartedMs = now;
    firestore.createDocument(asyncClient, Firestore::Parent(FIREBASE_PROJECT_ID), path,
                             DocumentMask("type"), doc, operationResult);
  }

  void startSessionGet(uint32_t now) {
    nextSessionCheckMs = now + NetworkConfig::REMOTE_SESSION_CHECK_INTERVAL_MS;
    recoveryRetryPending = false;
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
    recoveryRetryPending = false;
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
    json += ",\"discoveredBmsCount\":";
    json += system.discoveredBmsCount;
    json += ",\"connectedBmsCount\":";
    json += system.connectedBmsCount;
    json += ",\"onlineBmsCount\":";
    json += system.onlineBmsCount;
    json += '}';
    operationResult.clear();
    operation = Operation::StatusUpdate;
    operationInFlight = true;
    operationStartedMs = now;
    database.update<object_t>(asyncClient, deviceRootPath() + "/status",
                              object_t(json.c_str()), operationResult);
  }

  void startLiveUpdate(const SystemBatteryData& system, const BmsDeviceState* devices,
                       size_t deviceCount, uint32_t dataRevision,
                       uint32_t now, uint64_t epochMs) {
    String json;
    json.reserve(700 + deviceCount * 850);
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
    json += "},\"bms\":{";
    for (size_t i = 0; i < deviceCount; ++i) {
      if (i) json += ',';
      json += '\"';
      json += devices[i].id;
      json += "\":";
      appendBmsJson(json, devices[i], isBmsOnline(devices[i], now), now, epochMs);
    }
    json += '}';
    json += '}';
    livePayloadSize = json.length();
    pendingDataRevision = dataRevision;
    lastTelemetryAttemptMs = now;
    recoveryRetryPending = false;
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
                             const BmsDeviceState* devices, size_t deviceCount,
                             uint32_t dataRevision,
                             EventLogMonitor& eventLog, uint32_t now, uint64_t epochMs) {
  if (impl_->lastUpdateCallMs != 0) {
    const uint32_t gap = now - impl_->lastUpdateCallMs;
    if (gap > impl_->maxUpdateGapMs) impl_->maxUpdateGapMs = gap;
  }
  impl_->lastUpdateCallMs = now;
  impl_->eventLog = &eventLog;
  if (network.wifiConnected && !impl_->initialized) impl_->initialize();
  if (!impl_->initialized) return;

  impl_->app.loop();
  impl_->serviceQueueDrain(now);
  impl_->processAuthResult();
  const bool ready = network.wifiConnected && impl_->app.ready();
  network.firebaseReady = ready;
  impl_->trackAuthLifecycle(ready, now);
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
  impl_->updateRemoteSessionState(network, epochMs, now);
  impl_->serviceWatchdog(network, now, ready);
  impl_->printHealth(network, now, epochMs);
  if (!ready || impl_->operationInFlight || impl_->queueDraining ||
      static_cast<int32_t>(now - impl_->nextRecoveryMs) < 0) return;

  // Past every gate: the backoff has elapsed and the transport is free, so the
  // scheduled recovery retry is about to be (or imminently will be) issued
  // below. Release the re-entry guard now as a safety net - even if nothing is
  // due to send this instant, serviceWatchdog may resume evaluating staleness.
  impl_->recoveryRetryPending = false;

  if (network.timeSynced &&
      static_cast<int32_t>(now - impl_->nextSessionCheckMs) >= 0) {
    impl_->startSessionGet(now);
    return;
  }
  if (network.timeSynced && eventLog.pending() &&
      static_cast<int32_t>(now - impl_->nextLogWriteMs) >= 0) {
    impl_->startLogWrite(*eventLog.peek(), now, epochMs);
    return;
  }
  if (network.timeSynced &&
      static_cast<int32_t>(now - impl_->nextStatusMs) >= 0) {
    impl_->startStatusUpdate(network, system, now);
    return;
  }
  const bool freshSnapshot = dataRevision != impl_->uploadedDataRevision;
  if (network.remoteSessionActive && freshSnapshot &&
      now - impl_->lastTelemetryAttemptMs >= NetworkConfig::TELEMETRY_INTERVAL_MS) {
    impl_->startLiveUpdate(system, devices, deviceCount, dataRevision, now, epochMs);
  }
}

size_t FirebaseService::lastLivePayloadSize() const { return impl_->livePayloadSize; }
