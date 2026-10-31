#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\Motion.cpp"
#include "Motion.h"
#include "../generated/MotionAssets.h"
namespace routine::ui {
void MotionPlayer::observe(const application::ProductState& state,const application::ProductView& view,uint32_t now){
  const auto* record=view.routine>=0&&view.routine<state.count?&state.routines[size_t(view.routine)]:nullptr;
  const uint64_t run=record?record->run.spec.id:0;
  const bool changed=!seen_||screen_!=view.screen||run_!=run||version_!=view.expectedVersion||
      (application::isSummaryScreen(view.screen)&&(day_!=view.summary.day||page_!=view.item));
  if(changed){
    active_={};active_.character=state.companion.selected;
    active_.stage=domain::growthView(state.companion.experience[active_.character]).stage;started_=now;
    if(enabled_&&view.screen==application::Screen::AbandonConfirm&&assets::cryClips[active_.character*3+active_.stage-1].body)active_.kind=MotionKind::Crying;
    else if(enabled_&&seen_&&screen_==application::Screen::Letter&&view.screen==application::Screen::Routine&&run_==run)active_.kind=MotionKind::Letter;
    else if(enabled_&&seen_&&screen_==application::Screen::SummaryIntro&&view.screen==application::Screen::Summary&&
        day_==view.summary.day&&day_>=0&&record&&view.item==0)active_.kind=MotionKind::SummaryLetter;
    else if(enabled_&&seen_&&screen_==application::Screen::Feed&&!consumed_&&previous_.food&&
        (view.screen==application::Screen::Growth||view.screen==application::Screen::Evolution)){
      // Presentation follows an already committed meal. It never grants rewards.
      for(size_t i=0;i<state.count;++i)if(state.routines[i].run.spec.id==run_ && (state.routines[i].consumedRewards&(1u<<rewardStep_))){active_=previous_;active_.kind=MotionKind::Eating;break;}
    }
    if(enabled_&&active_.kind==MotionKind::None&&view.screen==application::Screen::Home){
      const auto* idle=assets::idleFrames[active_.character*3+active_.stage-1];
      if(idle[0]&&idle[1])active_.kind=MotionKind::Idle;
    }
  }
  screen_=view.screen;run_=run;version_=view.expectedVersion;day_=view.summary.day;page_=view.item;seen_=true;
  previous_.character=state.companion.selected;previous_.stage=domain::growthView(state.companion.experience[previous_.character]).stage;
  previous_.food=record&&view.rewardStep<record->run.spec.stepCount?record->food[view.rewardStep]:0;
  consumed_=record&&view.rewardStep<record->run.spec.stepCount&&(record->consumedRewards&(1u<<view.rewardStep));
  rewardStep_=view.rewardStep;
}
MotionFrame MotionPlayer::sample(uint32_t now)const{
  auto result=active_;const uint32_t elapsed=now-started_;
  switch(result.kind){
    case MotionKind::Letter:{
      if(elapsed>=uint32_t(assets::kLetterFrames+kLetterHoldFrames)*timing_.letterMs)return {};
      const uint32_t frame=elapsed/timing_.letterMs;
      result.frame=uint8_t(frame<assets::kLetterFrames?frame:assets::kLetterFrames-1);break;
    }
    case MotionKind::Idle:result.frame=uint8_t((elapsed/timing_.idleMs)%2);break;
    case MotionKind::SummaryLetter:if(elapsed>=6u*timing_.summaryFrameMs)return {};result.frame=uint8_t(elapsed/timing_.summaryFrameMs);break;
    case MotionKind::Eating:{const auto count=assets::biteClips[result.food-1].count+1u;if(elapsed>=count*timing_.eatingMs)return {};result.frame=uint8_t(elapsed/timing_.eatingMs);break;}
    case MotionKind::Crying:result.frame=uint8_t((elapsed/timing_.cryingMs)%5);break;
    case MotionKind::None:break;
  }
  return result;
}
}
