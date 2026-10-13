#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\input\\SystemGestures.h"
#pragma once
#include "ContextualGestures.h"

namespace routine::input {
// Delay the 800ms Confirm until release so a 5s setup hold cannot confirm a
// routine or feed a character on its way to opening network settings.
class SystemGestures {
 public:
  static constexpr uint32_t kSetupHoldMs=5000;
  void begin(uint32_t now,bool pressed,uint32_t context){
    gestures_.begin(now,pressed,context);raw_=stable_=pressed;armed_=!pressed;
    pressedAt_=changedAt_=now;pending_={};fired_=requested_=false;
  }
  InputEvent update(uint32_t now,bool pressed,uint32_t context){
    if(pressed!=raw_){raw_=pressed;changedAt_=now;}
    bool released=false;
    if(raw_!=stable_&&uint32_t(now-changedAt_)>=Button::kDebounceMs){
      stable_=raw_;
      if(stable_){pressedAt_=now;fired_=false;}else {released=true;armed_=true;}
    }
    if(!context||pending_.context!=context)pending_={};
    const auto event=gestures_.update(now,pressed,context);
    if(armed_&&stable_&&pressed&&!fired_&&uint32_t(now-pressedAt_)>=kSetupHoldMs){
      fired_=requested_=true;pending_={};gestures_.begin(now,true,context);return {};
    }
    if(released){
      if(pending_.intent!=Intent::None&&!fired_){const auto result=pending_;pending_={};return result;}
    }
    if(fired_)return {};
    if(event.intent==Intent::Confirm){pending_=event;return {};}
    return event;
  }
  bool takeSetup(){const bool value=requested_;requested_=false;return value;}
  bool busy()const{return gestures_.busy()||pending_.intent!=Intent::None;}
 private:
  ContextualGestures gestures_;
  InputEvent pending_{};
  uint32_t pressedAt_=0,changedAt_=0;
  bool raw_=false,stable_=false,armed_=true,fired_=false,requested_=false;
};
}
