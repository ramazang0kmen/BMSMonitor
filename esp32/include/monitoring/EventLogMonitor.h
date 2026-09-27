#pragma once

#include <Arduino.h>

#include "config/LogConfig.h"
#include "models/BatteryData.h"
#include "models/LogEvent.h"
#include "models/SystemBatteryData.h"

// Debounced two-threshold latch. `enter` must hold for enterHoldMs to latch
// (Entered); once latched, `clear` must hold for clearHoldMs to unlatch
// (Cleared). Cleared is only reported for a latch whose Entered was reported,
// so recovery entries always pair with a logged problem.
class ConditionLatch {
 public:
  enum class Edge : uint8_t { None, Entered, Cleared };

  // suppressInitial: when the very first sample already satisfies `enter`,
  // latch silently. Used for threshold crossings, which should log the moment
  // a value crosses the line - not on every reboot while it is already past it.
  Edge update(bool enter, bool clear, uint32_t now, uint32_t enterHoldMs,
              uint32_t clearHoldMs, bool suppressInitial);
  // The caller could not evaluate the condition this pass (data offline);
  // restart any hold timer instead of letting it run across the gap.
  void cancelPending() { pending_ = false; }
  uint32_t onsetMs() const { return onsetMs_; }

 private:
  bool initialized_ = false;
  bool latched_ = false;
  bool logged_ = false;
  bool everLogged_ = false;
  bool pending_ = false;
  uint32_t pendingSinceMs_ = 0;
  uint32_t onsetMs_ = 0;
  uint32_t lastLoggedMs_ = 0;
};

class EventLogMonitor {
 public:
  void recordBoot(uint32_t resetReason, uint32_t now);
  void update(const SystemBatteryData& system, const BmsDeviceState& bms1,
              const BmsDeviceState& bms2, uint32_t now);

  // Upload queue, oldest first. peek() stays valid until pop().
  LogEvent* peek();
  void pop();
  size_t pending() const { return count_; }
  uint32_t droppedCount() const { return dropped_; }

 private:
  struct BmsLatches {
    ConditionLatch socLow;
    ConditionLatch socEmpty;
    ConditionLatch dischargeOff;
    ConditionLatch chargeOff;
    ConditionLatch protection;
    ConditionLatch offline;
  };

  void capture(LogEvent& event, const SystemBatteryData& system,
               const BmsDeviceState* const bms[2], const bool online[2]) const;
  void emit(LogEventType type, LogSeverity severity, uint8_t bmsIndex, uint32_t capturedMs,
            uint32_t detail, const SystemBatteryData& system,
            const BmsDeviceState* const bms[2], const bool online[2]);
  void updateBms(size_t index, const SystemBatteryData& system,
                 const BmsDeviceState* const bms[2], const bool online[2], uint32_t now);
  void updateSystem(const SystemBatteryData& system, const BmsDeviceState* const bms[2],
                    const bool online[2], uint32_t now);

  LogEvent queue_[LogConfig::QUEUE_CAPACITY];
  size_t head_ = 0;
  size_t count_ = 0;
  uint32_t dropped_ = 0;

  bool bootPending_ = false;
  uint32_t bootMs_ = 0;
  uint32_t bootResetReason_ = 0;

  BmsLatches bmsLatches_[2];
  ConditionLatch avgSocLow_;
  ConditionLatch avgSocEmpty_;
  ConditionLatch avgSocFull_;
  ConditionLatch loadOff_;
  ConditionLatch socMismatch_;

  bool hasLoadedSnapshot_ = false;
  SystemSnapshot loadedSystem_;
  float loadedBmsVoltage_[2] = {};
};
