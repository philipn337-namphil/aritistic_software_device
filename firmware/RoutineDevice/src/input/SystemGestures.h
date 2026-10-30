#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\input\\SystemGestures.h"
#pragma once
#include "ContextualGestures.h"

namespace routine::input {
// A 5s hold of the Confirm (middle) button opens network settings. Taps fire
// only on a short release, so the hold itself never confirms a routine.
class SystemGestures {
 public:
  static constexpr uint32_t kSetupHoldMs=5000;
  static constexpr uint8_t kSetupMask=1u<<static_cast<uint8_t>(Control::Secondary);
  void begin(uint32_t now,uint8_t pressed,uint32_t context){
    gestures_.begin(now,pressed,context);
    const bool held=pressed&kSetupMask;raw_=stable_=held;armed_=!held;
    pressedAt_=changedAt_=now;fired_=requested_=false;
  }
  InputEvent update(uint32_t now,uint8_t pressed,uint32_t context){
    const bool held=pressed&kSetupMask;
    if(held!=raw_){raw_=held;changedAt_=now;}
    if(raw_!=stable_&&uint32_t(now-changedAt_)>=Button::kDebounceMs){
      stable_=raw_;
      if(stable_){pressedAt_=now;fired_=false;}else armed_=true;
    }
    const auto event=gestures_.update(now,pressed,context);
    if(armed_&&stable_&&held&&!fired_&&uint32_t(now-pressedAt_)>=kSetupHoldMs){
      fired_=requested_=true;gestures_.begin(now,pressed,context);return {};
    }
    if(fired_)return {};
    return event;
  }
  bool takeSetup(){const bool value=requested_;requested_=false;return value;}
  bool busy()const{return gestures_.busy();}
 private:
  ContextualGestures gestures_;
  uint32_t pressedAt_=0,changedAt_=0;
  bool raw_=false,stable_=false,armed_=true,fired_=false,requested_=false;
};
}
