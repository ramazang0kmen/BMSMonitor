#pragma once

#include <Arduino.h>

struct NetworkState {
  bool wifiConnected = false;
  bool firebaseReady = false;
  bool timeSynced = false;
  bool remoteViewerActive = false;
  int wifiRssi = 0;
  String ip;
  uint64_t viewerLastSeen = 0;
  uint64_t lastTelemetryUpload = 0;
  uint64_t lastStatusUpload = 0;
};
