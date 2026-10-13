#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\input\\Intent.h"
#pragma once
#include "Button.h"

namespace routine::input {
enum class Intent { None, Previous, Next, Confirm };
enum class Gesture { None, Tap, DoubleTap, Hold };
enum class Control { Primary, Secondary, Tertiary };

// Only these bindings change when the physical controls change.
inline Intent mapSingleButton(Control control, Gesture gesture) {
  if (control != Control::Primary) return Intent::None;
  switch (gesture) {
    case Gesture::Tap: return Intent::Next;
    case Gesture::DoubleTap: return Intent::Previous;
    case Gesture::Hold: return Intent::Confirm;
    default: return Intent::None;
  }
}
inline Intent mapThreeButtons(Control control, Gesture gesture) {
  if (gesture != Gesture::Tap) return Intent::None;
  switch (control) {
    case Control::Primary: return Intent::Previous;
    case Control::Secondary: return Intent::Confirm;
    case Control::Tertiary: return Intent::Next;
  }
  return Intent::None;
}

// Three-button mode uses one debouncer per pin and immediate taps. It does not
// need the single-button double-tap delay. Product handlers still receive Intent.
class Gestures {
 public:
  static constexpr uint32_t kDoubleTapMs = 300;
  void begin(uint32_t now, bool pressed) {
    button_.begin(now, pressed);
    pending_ = secondStarted_ = false;
  }
  Gesture update(uint32_t now, bool pressed) {
    Gesture due = Gesture::None;
    if (pending_ && !secondStarted_) {
      if (uint32_t(now - releasedAt_) >= kDoubleTapMs) {
        pending_ = false;
        due = Gesture::Tap;
      } else if (pressed) {
        secondStarted_ = true;
      }
    }
    const auto event = button_.update(now, pressed);
    if (event == ButtonEvent::LongPress) {
      pending_ = secondStarted_ = false;
      return Gesture::Hold;
    }
    if (event == ButtonEvent::ShortPress) {
      if (pending_ && secondStarted_) {
        pending_ = secondStarted_ = false;
        return Gesture::DoubleTap;
      }
      pending_ = true;
      secondStarted_ = false;
      releasedAt_ = now;
    }
    // A bounce must not keep a pending single tap waiting forever.
    if (secondStarted_ && !button_.busy()) secondStarted_ = false;
    return due;
  }
  bool busy() const { return button_.busy() || pending_; }
 private:
  Button button_;
  bool pending_ = false, secondStarted_ = false;
  uint32_t releasedAt_ = 0;
};
}
