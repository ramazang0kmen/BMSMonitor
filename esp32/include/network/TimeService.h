#pragma once

#include <Arduino.h>

#include "models/NetworkState.h"

class TimeService {
 public:
  void update(NetworkState& state);
  uint64_t epochMs() const;

 private:
  bool ntpStarted_ = false;
  bool synchronized_ = false;
};
