#include "network/TimeService.h"

#include <sys/time.h>
#include <time.h>

void TimeService::update(NetworkState& state) {
  if (state.wifiConnected && !ntpStarted_) {
    configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    ntpStarted_ = true;
  }
  if (!synchronized_ && ntpStarted_ && time(nullptr) > 1700000000) {
    synchronized_ = true;
    Serial.println("[TIME] NTP synchronized");
  }
  state.timeSynced = synchronized_;
}

uint64_t TimeService::epochMs() const {
  if (!synchronized_) return 0;
  timeval tv{};
  gettimeofday(&tv, nullptr);
  return static_cast<uint64_t>(tv.tv_sec) * 1000ULL +
         static_cast<uint64_t>(tv.tv_usec / 1000);
}
