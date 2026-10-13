#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\input\\Button.h"
#pragma once
#include <cstdint>

namespace routine::input {
enum class ButtonEvent { None, ShortPress, LongPress };

class Button {
 public:
  static constexpr uint32_t kDebounceMs = 30;
  static constexpr uint32_t kLongPressMs = 800;

  void begin(uint32_t now, bool pressed) {
    raw_ = stable_ = pressed;
    changedAt_ = pressedAt_ = now;
    armed_ = !pressed;
    longSent_ = false;
  }

  ButtonEvent update(uint32_t now, bool pressed) {
    if (pressed != raw_) { raw_ = pressed; changedAt_ = now; }
    if (stable_ != raw_ && uint32_t(now - changedAt_) >= kDebounceMs) {
      stable_ = raw_;
      if (stable_) {
        pressedAt_ = now;
        longSent_ = false;
      } else {
        const bool shortPress = armed_ && !longSent_;
        armed_ = true;  // A button held during boot must first be released.
        return shortPress ? ButtonEvent::ShortPress : ButtonEvent::None;
      }
    }
    if (armed_ && stable_ && raw_ && !longSent_ &&
        uint32_t(now - pressedAt_) >= kLongPressMs) {
      longSent_ = true;
      return ButtonEvent::LongPress;
    }
    return ButtonEvent::None;
  }

  bool busy() const { return raw_ || stable_; }

 private:
  bool raw_ = false, stable_ = false, armed_ = true, longSent_ = false;
  uint32_t changedAt_ = 0, pressedAt_ = 0;
};

class BuzzerPulse {
 public:
  void start(uint32_t now, uint32_t durationMs = 80) {
    startedAt_ = now;
    duration_ = durationMs;
    active_ = durationMs != 0;
  }
  bool update(uint32_t now) {
    if (active_ && uint32_t(now - startedAt_) >= duration_) active_ = false;
    return active_;
  }
  bool active() const { return active_; }
 private:
  uint32_t startedAt_ = 0, duration_ = 0;
  bool active_ = false;
};
}
