#pragma once

#include <Arduino.h>

enum class LogEventType : uint8_t {
  Boot,
  AvgSocLow,
  AvgSocEmpty,
  AvgSocFull,
  BmsSocLow,
  BmsSocEmpty,
  DischargeStopped,
  DischargeStarted,
  BmsDischargeOff,
  BmsDischargeOn,
  BmsChargeOff,
  BmsChargeOn,
  BmsProtection,
  BmsProtectionCleared,
  BmsOffline,
  BmsOnline,
  SocMismatch,
};

enum class LogSeverity : uint8_t { Info, Warning, Error };

struct BmsSnapshot {
  bool online = false;
  uint8_t soc = 0;
  float voltage = 0.0f;
  float current = 0.0f;
  float power = 0.0f;
  float remainingAh = 0.0f;
  float minCellVoltage = 0.0f;
  float maxCellVoltage = 0.0f;
  float temperature = 0.0f;
  bool chargeMos = false;
  bool dischargeMos = false;
  uint16_t protectionFlags = 0;
};

struct SystemSnapshot {
  float voltage = 0.0f;
  float current = 0.0f;
  float power = 0.0f;
  float soc = 0.0f;
  float remainingAh = 0.0f;
  float socDelta = 0.0f;
};

struct LogEvent {
  LogEventType type = LogEventType::Boot;
  LogSeverity severity = LogSeverity::Info;
  // 0 = the whole system, 1/2 = the BMS the event is about.
  uint8_t bmsIndex = 0;
  uint8_t attempts = 0;
  // millis() when the event was detected; converted to wall-clock time when
  // it is uploaded so events captured before NTP sync still get a real time.
  uint32_t capturedMs = 0;
  // Type-specific extra value: reset reason for Boot, protection flags for
  // BmsProtection.
  uint32_t detail = 0;
  SystemSnapshot system;
  BmsSnapshot bms[2];
  // DischargeStopped only: the last reading taken while the load was still
  // on, i.e. the voltage under load right before it was cut.
  bool hasBefore = false;
  SystemSnapshot before;
  float beforeBmsVoltage[2] = {};
};

const char* logEventTypeName(LogEventType type);
const char* logSeverityName(LogSeverity severity);
