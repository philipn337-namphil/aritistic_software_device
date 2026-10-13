#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\ClockSync.h"
#pragma once
#include <cstdint>

namespace routine::application {
enum class SyncState { NeedsSetup, Connecting, AwaitingTime, Current, RetryWait, Suspended };
struct SyncActions {
  bool connect = false;
  bool requestTime = false;
  bool stopTime = false;
};
// Owns retry/freshness policy, with no Wi-Fi, wall-clock or RTOS dependencies.
class ClockSync {
 public:
  static constexpr uint32_t kConnectTimeoutMs = 15000;
  static constexpr uint32_t kTimeTimeoutMs = 15000;
  static constexpr uint32_t kRetryMs = 30000;
  SyncActions request(uint32_t now, bool configured, bool connected) {
    fresh_ = false;
    since_ = now;
    if (!configured) { state_ = SyncState::NeedsSetup; return {false, false, true}; }
    state_ = connected ? SyncState::AwaitingTime : SyncState::Connecting;
    return {!connected, connected, true};
  }
  SyncActions tick(uint32_t now, bool connected, bool receivedNewTime) {
    if (state_ == SyncState::Current && !connected) return request(now, true, false);
    if (state_ == SyncState::AwaitingTime && receivedNewTime) {
      known_ = fresh_ = true;
      state_ = SyncState::Current;
      return {false, false, true};
    }
    if (state_ == SyncState::Connecting && connected) {
      state_ = SyncState::AwaitingTime;
      since_ = now;
      return {false, true, false};
    }
    if ((state_ == SyncState::Connecting && uint32_t(now - since_) >= kConnectTimeoutMs) ||
        (state_ == SyncState::AwaitingTime && (!connected || uint32_t(now - since_) >= kTimeTimeoutMs))) {
      state_ = SyncState::RetryWait;
      since_ = now;
      return {false, false, true};
    }
    if (state_ == SyncState::RetryWait && uint32_t(now - since_) >= kRetryMs)
      return request(now, true, connected);
    return {};
  }
  SyncActions suspend() {
    fresh_ = false;
    state_ = SyncState::Suspended;
    return {false, false, true};
  }
  bool known() const { return known_; }
  bool fresh() const { return fresh_; }
  bool busy() const { return state_ == SyncState::Connecting || state_ == SyncState::AwaitingTime; }
  SyncState state() const { return state_; }
 private:
  SyncState state_ = SyncState::NeedsSetup;
  bool known_ = false, fresh_ = false;
  uint32_t since_ = 0;
};
}
