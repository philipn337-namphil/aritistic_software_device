#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\input\\ContextualGestures.h"
#pragma once
#include "Intent.h"

namespace routine::input {
struct InputEvent { Intent intent=Intent::None; uint32_t context=0; };
// A changed screen invalidates both queued intents and an unfinished tap/hold.
// Context 0 blocks product input (e.g. while a new screen is being sent).
class ContextualGestures {
 public:
  // `pressed` is a bit mask of the three buttons (see ThreeButtonGestures).
  void begin(uint32_t now,uint8_t pressed,uint32_t context){context_=context;gestures_.begin(now,pressed);}
  InputEvent update(uint32_t now,uint8_t pressed,uint32_t context){
    if(!context||context!=context_){begin(now,pressed,context);return {};}
    return {gestures_.update(now,pressed),context_};
  }
  bool busy()const{return gestures_.busy();}
 private:
  ThreeButtonGestures gestures_;
  uint32_t context_=0;
};
}
