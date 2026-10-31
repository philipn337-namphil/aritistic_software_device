#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\Motion.h"
#pragma once
#include "../application/ProductView.h"
namespace routine::ui {
enum class MotionKind : uint8_t { None, Letter, Eating, Crying, SummaryLetter, Idle };
inline bool isTransient(MotionKind kind) {
  return kind==MotionKind::Letter || kind==MotionKind::Eating || kind==MotionKind::SummaryLetter;
}
struct MotionFrame { MotionKind kind=MotionKind::None; uint8_t frame=0,character=0,stage=1,food=0; };
// Timing is a provisional presentation setting, not a product/domain rule.
struct MotionTiming { uint16_t letterMs=100,eatingMs=260,cryingMs=180,summaryFrameMs=180,idleMs=600; };
// The last letter frame (fully open) is held this many ticks before the routine screen.
constexpr uint8_t kLetterHoldFrames=4;
class MotionPlayer {
 public:
  void observe(const application::ProductState&,const application::ProductView&,uint32_t now);
  MotionFrame sample(uint32_t now) const;
  void enable(bool value){if(enabled_!=value){enabled_=value;seen_=false;skip();}}
  bool enabled()const{return enabled_;}
  void skip(){active_.kind=MotionKind::None;}
  bool transient(uint32_t now)const{return isTransient(sample(now).kind);}
  void timing(MotionTiming value){if(value.letterMs&&value.eatingMs&&value.cryingMs&&value.summaryFrameMs&&value.idleMs){timing_=value;seen_=false;skip();}}
 private:
  MotionTiming timing_{};
  MotionFrame active_{},previous_{};
  application::Screen screen_=application::Screen::Home;
  uint64_t run_=0;
  int64_t day_=application::kUnknownDay;
  uint8_t page_=0;
  uint32_t started_=0,version_=0;
  uint8_t rewardStep_=0;
  bool seen_=false,enabled_=true,consumed_=false;
};
}
