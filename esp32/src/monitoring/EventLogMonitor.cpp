#include "monitoring/EventLogMonitor.h"

#include <esp_system.h>

namespace {
constexpr uint32_t BOOT_SNAPSHOT_WAIT_MS = 30000;

bool isCrashReset(uint32_t reason) {
  switch (static_cast<esp_reset_reason_t>(reason)) {
    case ESP_RST_PANIC:
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:
    case ESP_RST_BROWNOUT:
      return true;
    default:
      return false;
  }
}

SystemSnapshot toSnapshot(const SystemBatteryData& system) {
  SystemSnapshot snapshot;
  snapshot.voltage = system.voltage;
  snapshot.current = system.current;
  snapshot.power = system.power;
  snapshot.soc = system.soc;
  snapshot.remainingAh = system.remainingAh;
  snapshot.socDelta = system.socDelta;
  return snapshot;
}
}  // namespace

const char* logEventTypeName(LogEventType type) {
  switch (type) {
    case LogEventType::Boot: return "boot";
    case LogEventType::AvgSocLow: return "avg_soc_low";
    case LogEventType::AvgSocEmpty: return "avg_soc_empty";
    case LogEventType::AvgSocFull: return "avg_soc_full";
    case LogEventType::BmsSocLow: return "bms_soc_low";
    case LogEventType::BmsSocEmpty: return "bms_soc_empty";
    case LogEventType::DischargeStopped: return "discharge_stopped";
    case LogEventType::DischargeStarted: return "discharge_started";
    case LogEventType::BmsDischargeOff: return "bms_discharge_off";
    case LogEventType::BmsDischargeOn: return "bms_discharge_on";
    case LogEventType::BmsChargeOff: return "bms_charge_off";
    case LogEventType::BmsChargeOn: return "bms_charge_on";
    case LogEventType::BmsProtection: return "bms_protection";
    case LogEventType::BmsProtectionCleared: return "bms_protection_cleared";
    case LogEventType::BmsOffline: return "bms_offline";
    case LogEventType::BmsOnline: return "bms_online";
    case LogEventType::SocMismatch: return "soc_mismatch";
  }
  return "unknown";
}

const char* logSeverityName(LogSeverity severity) {
  switch (severity) {
    case LogSeverity::Warning: return "warning";
    case LogSeverity::Error: return "error";
    default: return "info";
  }
}

ConditionLatch::Edge ConditionLatch::update(bool enter, bool clear, uint32_t now,
                                            uint32_t enterHoldMs, uint32_t clearHoldMs,
                                            bool suppressInitial) {
  if (!initialized_) {
    initialized_ = true;
    if (suppressInitial && enter) {
      latched_ = true;
      return Edge::None;
    }
  }
  const bool condition = latched_ ? clear : enter;
  if (!condition) {
    pending_ = false;
    return Edge::None;
  }
  if (!pending_) {
    pending_ = true;
    pendingSinceMs_ = now;
  }
  if (now - pendingSinceMs_ < (latched_ ? clearHoldMs : enterHoldMs)) return Edge::None;
  pending_ = false;

  if (latched_) {
    latched_ = false;
    const bool wasLogged = logged_;
    logged_ = false;
    onsetMs_ = pendingSinceMs_;
    return wasLogged ? Edge::Cleared : Edge::None;
  }
  latched_ = true;
  if (everLogged_ && now - lastLoggedMs_ < LogConfig::EVENT_COOLDOWN_MS) {
    logged_ = false;
    return Edge::None;
  }
  logged_ = true;
  everLogged_ = true;
  lastLoggedMs_ = now;
  onsetMs_ = pendingSinceMs_;
  return Edge::Entered;
}

void EventLogMonitor::recordBoot(uint32_t resetReason, uint32_t now) {
  // Held back until the BMS links are up (or a timeout passes) so the boot
  // entry carries the battery state the gateway woke up to.
  bootPending_ = true;
  bootMs_ = now;
  bootResetReason_ = resetReason;
}

void EventLogMonitor::capture(LogEvent& event, const SystemBatteryData& system,
                              const BmsDeviceState* const bms[2], const bool online[2]) const {
  event.system = toSnapshot(system);
  for (size_t i = 0; i < 2; ++i) {
    const BatteryData& data = bms[i]->data;
    BmsSnapshot& out = event.bms[i];
    out.online = online[i];
    out.soc = data.soc;
    out.voltage = data.voltage;
    out.current = data.current;
    out.power = data.power;
    out.remainingAh = data.remainingCapacityAh;
    out.minCellVoltage = data.minCellVoltage;
    out.maxCellVoltage = data.maxCellVoltage;
    out.temperature = data.temperatureCount ? data.temperatures[0] : 0.0f;
    out.chargeMos = data.chargeMos;
    out.dischargeMos = data.dischargeMos;
    out.protectionFlags = data.protectionFlags;
  }
}

void EventLogMonitor::emit(LogEventType type, LogSeverity severity, uint8_t bmsIndex,
                           uint32_t capturedMs, uint32_t detail,
                           const SystemBatteryData& system,
                           const BmsDeviceState* const bms[2], const bool online[2]) {
  if (count_ == LogConfig::QUEUE_CAPACITY) {
    ++dropped_;
    Serial.printf("[LOG][WARN] Queue full, dropped %s (total dropped %lu)\n",
                  logEventTypeName(type), static_cast<unsigned long>(dropped_));
    return;
  }
  LogEvent& event = queue_[(head_ + count_) % LogConfig::QUEUE_CAPACITY];
  event = LogEvent();
  event.type = type;
  event.severity = severity;
  event.bmsIndex = bmsIndex;
  event.capturedMs = capturedMs;
  event.detail = detail;
  capture(event, system, bms, online);
  if (type == LogEventType::DischargeStopped && hasLoadedSnapshot_) {
    event.hasBefore = true;
    event.before = loadedSystem_;
    event.beforeBmsVoltage[0] = loadedBmsVoltage_[0];
    event.beforeBmsVoltage[1] = loadedBmsVoltage_[1];
  }
  ++count_;
  Serial.printf("[LOG] %s (%s) bms=%u | avg %.2fV %.1f%% | BMS1 %.2fV %u%% | BMS2 %.2fV %u%% | %.2fA\n",
                logEventTypeName(type), logSeverityName(severity), bmsIndex,
                event.system.voltage, event.system.soc, event.bms[0].voltage, event.bms[0].soc,
                event.bms[1].voltage, event.bms[1].soc, event.system.current);
}

void EventLogMonitor::update(const SystemBatteryData& system, const BmsDeviceState& bms1,
                             const BmsDeviceState& bms2, uint32_t now) {
  const BmsDeviceState* const bms[2] = {&bms1, &bms2};
  const bool online[2] = {system.bms1Online, system.bms2Online};

  if (bootPending_ && ((online[0] && online[1]) || now - bootMs_ >= BOOT_SNAPSHOT_WAIT_MS)) {
    bootPending_ = false;
    emit(LogEventType::Boot, isCrashReset(bootResetReason_) ? LogSeverity::Error : LogSeverity::Info,
         0, bootMs_, bootResetReason_, system, bms, online);
  }
  for (size_t i = 0; i < 2; ++i) updateBms(i, system, bms, online, now);
  updateSystem(system, bms, online, now);
}

void EventLogMonitor::updateBms(size_t index, const SystemBatteryData& system,
                                const BmsDeviceState* const bms[2], const bool online[2],
                                uint32_t now) {
  BmsLatches& latches = bmsLatches_[index];
  const uint8_t bmsNumber = static_cast<uint8_t>(index + 1);
  using Edge = ConditionLatch::Edge;

  // Offline detection is the one check that must run while the data is stale.
  // Not suppressed at boot: a BMS that never comes up is worth an entry.
  const Edge offline = latches.offline.update(!online[index], online[index], now,
                                              LogConfig::BMS_OFFLINE_HOLD_MS, 0, false);
  if (offline == Edge::Entered) {
    emit(LogEventType::BmsOffline, LogSeverity::Warning, bmsNumber, latches.offline.onsetMs(), 0,
         system, bms, online);
  } else if (offline == Edge::Cleared) {
    emit(LogEventType::BmsOnline, LogSeverity::Info, bmsNumber, now, 0, system, bms, online);
  }

  if (!online[index]) {
    latches.socLow.cancelPending();
    latches.socEmpty.cancelPending();
    latches.dischargeOff.cancelPending();
    latches.chargeOff.cancelPending();
    latches.protection.cancelPending();
    return;
  }

  const BatteryData& data = bms[index]->data;
  const float soc = data.soc;

  if (latches.socLow.update(soc <= LogConfig::SOC_LOW_PCT, soc >= LogConfig::SOC_LOW_REARM_PCT, now,
                            LogConfig::SOC_HOLD_MS, LogConfig::SOC_HOLD_MS, true) == Edge::Entered) {
    emit(LogEventType::BmsSocLow, LogSeverity::Warning, bmsNumber, latches.socLow.onsetMs(), 0,
         system, bms, online);
  }
  if (latches.socEmpty.update(data.soc == 0, soc >= LogConfig::SOC_EMPTY_REARM_PCT, now,
                              LogConfig::SOC_HOLD_MS, LogConfig::SOC_HOLD_MS, true) == Edge::Entered) {
    emit(LogEventType::BmsSocEmpty, LogSeverity::Error, bmsNumber, latches.socEmpty.onsetMs(), 0,
         system, bms, online);
  }

  // MOS and protection states are problems in their own right, so they are
  // logged even when already present at boot.
  const Edge dischargeOff = latches.dischargeOff.update(
      !data.dischargeMos, data.dischargeMos, now, LogConfig::MOS_HOLD_MS, LogConfig::MOS_HOLD_MS, false);
  if (dischargeOff == Edge::Entered) {
    emit(LogEventType::BmsDischargeOff, LogSeverity::Error, bmsNumber,
         latches.dischargeOff.onsetMs(), 0, system, bms, online);
  } else if (dischargeOff == Edge::Cleared) {
    emit(LogEventType::BmsDischargeOn, LogSeverity::Info, bmsNumber,
         latches.dischargeOff.onsetMs(), 0, system, bms, online);
  }

  const Edge chargeOff = latches.chargeOff.update(
      !data.chargeMos, data.chargeMos, now, LogConfig::MOS_HOLD_MS, LogConfig::MOS_HOLD_MS, false);
  if (chargeOff == Edge::Entered) {
    emit(LogEventType::BmsChargeOff, LogSeverity::Warning, bmsNumber, latches.chargeOff.onsetMs(),
         0, system, bms, online);
  } else if (chargeOff == Edge::Cleared) {
    emit(LogEventType::BmsChargeOn, LogSeverity::Info, bmsNumber, latches.chargeOff.onsetMs(), 0,
         system, bms, online);
  }

  const bool fault = data.protectionFlags != 0;
  const Edge protection = latches.protection.update(fault, !fault, now,
                                                    LogConfig::PROTECTION_HOLD_MS,
                                                    LogConfig::PROTECTION_HOLD_MS, false);
  if (protection == Edge::Entered) {
    emit(LogEventType::BmsProtection, LogSeverity::Error, bmsNumber, latches.protection.onsetMs(),
         data.protectionFlags, system, bms, online);
  } else if (protection == Edge::Cleared) {
    emit(LogEventType::BmsProtectionCleared, LogSeverity::Info, bmsNumber,
         latches.protection.onsetMs(), 0, system, bms, online);
  }
}

void EventLogMonitor::updateSystem(const SystemBatteryData& system,
                                   const BmsDeviceState* const bms[2], const bool online[2],
                                   uint32_t now) {
  using Edge = ConditionLatch::Edge;
  // With one BMS missing the "average" is just the other battery and total
  // current loses half the bank, so system-level events need both.
  if (!online[0] || !online[1]) {
    avgSocLow_.cancelPending();
    avgSocEmpty_.cancelPending();
    avgSocFull_.cancelPending();
    loadOff_.cancelPending();
    socMismatch_.cancelPending();
    return;
  }

  const float soc = system.soc;
  if (avgSocLow_.update(soc <= LogConfig::SOC_LOW_PCT, soc >= LogConfig::SOC_LOW_REARM_PCT, now,
                        LogConfig::SOC_HOLD_MS, LogConfig::SOC_HOLD_MS, true) == Edge::Entered) {
    emit(LogEventType::AvgSocLow, LogSeverity::Warning, 0, avgSocLow_.onsetMs(), 0, system, bms,
         online);
  }
  if (avgSocEmpty_.update(soc < LogConfig::SOC_EMPTY_PCT, soc >= LogConfig::SOC_EMPTY_REARM_PCT,
                          now, LogConfig::SOC_HOLD_MS, LogConfig::SOC_HOLD_MS,
                          true) == Edge::Entered) {
    emit(LogEventType::AvgSocEmpty, LogSeverity::Error, 0, avgSocEmpty_.onsetMs(), 0, system, bms,
         online);
  }
  if (avgSocFull_.update(soc >= LogConfig::SOC_FULL_PCT, soc <= LogConfig::SOC_FULL_REARM_PCT, now,
                         LogConfig::SOC_HOLD_MS, LogConfig::SOC_HOLD_MS, true) == Edge::Entered) {
    emit(LogEventType::AvgSocFull, LogSeverity::Info, 0, avgSocFull_.onsetMs(), 0, system, bms,
         online);
  }

  const float draw = LogConfig::DISCHARGE_CURRENT_SIGN * system.current;
  if (draw >= LogConfig::LOAD_ACTIVE_A) {
    hasLoadedSnapshot_ = true;
    loadedSystem_ = toSnapshot(system);
    loadedBmsVoltage_[0] = bms[0]->data.voltage;
    loadedBmsVoltage_[1] = bms[1]->data.voltage;
  }
  // Latched = "no load". Entered is the load dropping to zero (inverter or
  // BMS cut, or solar taking over); Cleared is it coming back afterwards,
  // which records the voltage the inverter restarts at.
  const Edge load = loadOff_.update(draw < LogConfig::LOAD_IDLE_A, draw >= LogConfig::LOAD_ACTIVE_A,
                                    now, LogConfig::LOAD_HOLD_MS, LogConfig::LOAD_HOLD_MS, true);
  if (load == Edge::Entered) {
    emit(LogEventType::DischargeStopped, LogSeverity::Warning, 0, loadOff_.onsetMs(), 0, system,
         bms, online);
  } else if (load == Edge::Cleared) {
    emit(LogEventType::DischargeStarted, LogSeverity::Info, 0, loadOff_.onsetMs(), 0, system, bms,
         online);
  }

  if (socMismatch_.update(system.socMismatch, system.socDelta < LogConfig::SOC_MISMATCH_REARM_PCT,
                          now, LogConfig::SOC_MISMATCH_HOLD_MS, LogConfig::SOC_MISMATCH_HOLD_MS,
                          true) == Edge::Entered) {
    emit(LogEventType::SocMismatch, LogSeverity::Warning, 0, socMismatch_.onsetMs(), 0, system,
         bms, online);
  }
}

LogEvent* EventLogMonitor::peek() { return count_ ? &queue_[head_] : nullptr; }

void EventLogMonitor::pop() {
  if (!count_) return;
  head_ = (head_ + 1) % LogConfig::QUEUE_CAPACITY;
  --count_;
}
