#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\ProductRenderer.cpp"
#include "ProductRenderer.h"
#include "Drawing.h"
#include "CatalogRenderer.h"
#include "SummaryRenderer.h"
#include "../generated/CatalogAssets.h"
#include "../generated/ThemeAssets.h"
#include "../generated/ProductAssets.h"
#include "../generated/MotionAssets.h"
#include "../content/Catalog.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace routine::ui {
namespace {
using namespace drawing;
using application::Screen;
constexpr uint16_t white = 0xffff, cream = 0xffbc;
void pill(uint16_t* dst, int x, int y, const char* label, uint16_t color = 0x3186) {
  rect(dst,x,y,86,39,color,255,19); text(dst,label,x+43,y+25,20,white);
}
void modal(uint16_t* dst, const char* prompt, bool pair, uint8_t selected, bool abandon,
           uint16_t panelColor, const assets::CompressedImage* character = nullptr, const assets::CryClip* cry=nullptr,uint8_t frame=1,const assets::Font* promptFont = nullptr) {
  // Figma backdrop: white veil and a small separable box blur within the panel.
  uint16_t row[kWidth];
  for (int y = 28; y < kHeight; ++y) {
    for (int x = 0; x < kWidth; ++x) {
      unsigned r=0,g=0,b=0,n=0;
      for (int dx=-2;dx<=2;++dx) if(x+dx>=0&&x+dx<kWidth) {const auto c=dst[y*kWidth+x+dx];r+=c>>11;g+=(c>>5)&63;b+=c&31;++n;}
      row[x]=uint16_t(((r/n)<<11)|((g/n)<<5)|(b/n));
    }
    std::memcpy(dst+y*kWidth,row,sizeof(row));
  }
  uint16_t column[kHeight];
  for (int x=0;x<kWidth;++x) {
    for (int y=28;y<kHeight;++y) {
      unsigned r=0,g=0,b=0,n=0;
      for(int dy=-2;dy<=2;++dy)if(y+dy>=28&&y+dy<kHeight){const auto c=dst[(y+dy)*kWidth+x];r+=c>>11;g+=(c>>5)&63;b+=c&31;++n;}
      column[y]=uint16_t(((r/n)<<11)|((g/n)<<5)|(b/n));
    }
    for(int y=28;y<kHeight;++y)dst[y*kWidth+x]=column[y];
  }
  rect(dst,0,28,240,292,white,84);
  if(abandon && cry && cry->body){
    image(dst,*cry->body);const auto& tear=cry->tears[frame<5?frame:1];
    if(tear.w&&tear.h)drawImageAt(dst,assets::tearSprite,tear.x,tear.y,tear.w,tear.h);
  } else if (abandon && character) {
    const int height=153,width=int(character->width)*height/character->height;
    drawImageAt(dst,*character,120-width/2,52,width,height);
  }
  const int y = pair ? 199 : 206;
  rect(dst,0,y,240,320-y,panelColor);
  if (promptFont) text(dst,*promptFont,prompt,120,y+37,cream);
  else text(dst,prompt,120,y+37,24,cream);
  if (pair) { pill(dst,24,y+55,selected==0?">취소":"취소"); pill(dst,131,y+55,selected==1?">확인":"확인"); }
  else pill(dst,78,261,">확인",0x2124);
}
void clock(uint16_t* dst, bool known, unsigned minutes) {
  char label[20]; const unsigned hour = (minutes/60)%24;
  if (known) std::snprintf(label,sizeof(label),"%s %u:%02u",hour<12?"AM":"PM",hour%12?hour%12:12,minutes%60);
  else std::snprintf(label,sizeof(label),"--:--");
  text(dst,label,8,20,16,white,false);
}
void greeting(uint16_t* dst, const char* label) {
  rect(dst,20,42,200,49,white,255,10);
  int size=20;
  while (size>14 && width(label,size)>188) --size;
  text(dst,label,120,75,size,0);
}
void growthPanel(uint16_t* dst, const assets::CharacterTheme& theme, const domain::Companion& companion, bool button) {
  const auto growth=domain::growthView(companion.experience[companion.selected]);
  char label[48];
  std::snprintf(label,sizeof(label),"%u단계  %u/%u",unsigned(growth.stage),unsigned(growth.progress),unsigned(growth.target));
  rect(dst,0,button?234:267,240,button?86:53,theme.button);
  text(dst,label,120,button?255:285,18,white);
  rect(dst,40,button?260:291,160,8,white,255,3);
  rect(dst,40,button?260:291,int(160*growth.progress/growth.target),8,theme.accent,255,3);
  if (button) pill(dst,77,275,">확인");
}
void renderRoutine(uint16_t* dst, const assets::CharacterTheme& theme, const application::ProductState& state, const application::ProductView& view, const application::RoutineRecord* record,MotionFrame motion) {
  const auto screen = view.screen;
  image(dst,*theme.background);
  image(dst,*theme.paper);
  if (record) {
    char title[112]; std::snprintf(title,sizeof(title),"<%s %u/%u>",record->title.data(),unsigned(domain::nextStep(record->run)+1),unsigned(record->run.spec.stepCount));
    block(dst,title,width(title,20)<=190?67:57,20,width(title,20)<=190?1:2);
    const auto daily = application::dailyRoutineProgress(state,size_t(view.routine));
    char progress[32]; std::snprintf(progress,sizeof(progress),"전체 %u/%u",unsigned(daily.ordinal),unsigned(daily.total));
    text(dst,progress,120,123,15,0);
    if (domain::nextStep(record->run) < record->run.spec.stepCount) block(dst,record->stepNames[domain::nextStep(record->run)].data(),160,26,3);
  }
  pill(dst,25,254,view.selected==0?">포기":"포기",theme.routineButton); pill(dst,132,254,view.selected==1?">완료":"완료",theme.routineButton);
  if (view.warning && screen == Screen::Routine) text(dst,"마감이 다가와요",120,43,14,white);
  if (screen == Screen::CompleteConfirm) modal(dst,"완료 하시겠습니까?",false,view.selected,false,theme.button);
  if (screen == Screen::AbandonConfirm) {
    const auto stage=domain::growthView(state.companion.experience[state.companion.selected]).stage;
    const auto index=state.companion.selected*3+stage-1;
    modal(dst,"진짜로...?",true,view.selected,true,theme.button,assets::homeCharacterArt[index],&assets::cryClips[index],motion.kind==MotionKind::Crying?motion.frame:1);
  }
}
void renderCatalogChoice(uint16_t* dst, const assets::CharacterTheme& theme, const application::ProductState& state,const application::ProductView& view) {
  const auto character=state.companion.selected;
  const auto stage=domain::growthView(state.companion.experience[character]).stage;
  image(dst,*theme.background);image(dst,*theme.homeBase);image(dst,*assets::homeCharacterArt[character*3+stage-1]);
  text(dst,assets::greetingFont,"좋은 하루 보내~",120,75,theme.greeting);
  rect(dst,0,199,240,121,theme.button); text(dst,assets::catalogPromptFont,"도감을 선택해주세요!",120,237,cream);
  pill(dst,24,254,view.selected==0?">캐릭터":"캐릭터",0x2965); pill(dst,131,254,view.selected==1?">먹이":"먹이",0x2965);
}
void renderCharacterCatalog(uint16_t* dst, const application::ProductState& state, const application::ProductView& view) {
  renderCatalogPage(dst,state,view);
}
void renderReward(uint16_t* dst, const assets::CharacterTheme& theme, const application::RoutineRecord* record, const application::ProductView& view) {
  const auto screen = view.screen;
  const auto food=record && view.rewardStep<record->run.spec.stepCount ? record->food[view.rewardStep] : 0;
  greeting(dst,screen==Screen::StepSaved?"먹이를 얻었어!":"맛있게 먹을게~");
  if (food>=1 && food<=content::kFoodCount) image(dst,*assets::rewardFoodArt[food-1]);
  rect(dst,0,254,240,66,theme.button);
  text(dst,content::foodName(food),120,272,18,cream);
  pill(dst,77,279,">확인");
}
void renderGrowth(uint16_t* dst, const assets::CharacterTheme& theme, const application::ProductState& state, const application::ProductView& view) {
  const auto screen = view.screen;
  greeting(dst,screen==Screen::Evolution ? state.companion.notice==domain::GrowthNotice::Completed ? "끝까지 함께 자랐어!" : "축하해! 진화했어!" : state.companion.experience[state.companion.selected] ? "먹이를 먹고 자랐어!" : "반가워! 함께 자라자~");
  growthPanel(dst, theme,state.companion,true);
}
void renderNewCharacter(uint16_t* dst, const assets::CharacterTheme& theme, const application::ProductView& view) {
  greeting(dst,"새 친구를 만나볼까?");
  rect(dst,25,97,190,113,theme.button,255,12); text(dst,"???",120,163,32,white);
  rect(dst,0,211,240,109,theme.button); text(dst,"선택하시겠습니까?",120,244,22,cream);
  pill(dst,24,265,view.selected==0?">취소":"취소"); pill(dst,131,265,view.selected==1?">확인":"확인");
}
void renderMessage(uint16_t* dst, const assets::CharacterTheme& theme, const application::RoutineRecord* record, const application::ProductView& view) {
  const auto screen = view.screen;
  const char* title = "";
  const char* detail = "";
  switch (screen) {
    case Screen::Result:
      if (record && record->run.phase == domain::Phase::Completed) title = "미션 완료! 축하해";
      else if (record && record->run.phase == domain::Phase::Abandoned) title = "괜찮아! 다음에 해보자";
      else title = "시간이 끝났어요";
      detail = "기록을 저장했어요";
      break;
    case Screen::ClockRequired:
      title = "시간을 확인하고 있어요";
      detail = "Wi-Fi 연결을 기다려요";
      break;
    case Screen::StorageError:
      title = "기록을 확인해야 해요";
      detail = "진행을 잠시 멈췄어요";
      break;
    default: break;
  }
  rect(dst,0,199,240,121,theme.button);
  block(dst,title,215,22,1,cream);
  text(dst,detail,120,260,16,white);
  if (screen != Screen::StorageError && screen != Screen::ClockRequired) pill(dst,77,275,">확인");
}
}
void renderDeviceStatus(uint16_t* dst,bool wifiConnected,int batteryPercent) {
  if(!dst)return;
  const uint16_t background=dst[0];
  // Retain Figma's battery outline; replace the sample full fill with real data.
  drawing::rect(dst,207,8,16,10,background);
  if(batteryPercent<0)drawing::text(dst,assets::timeFont,"?",215,19,0xffff);
  else drawing::rect(dst,207,8,16*std::min(100,batteryPercent)/100,10,0xffff);
  if(!wifiConnected){
    // A slash makes the retained connection icon truthful when offline.
    for(int i=0;i<18;++i)drawing::rect(dst,178+i,5+i,2,2,background);
    for(int i=0;i<18;++i)drawing::rect(dst,180+i,5+i,1,1,0xffff);
  }
}
void renderProduct(uint16_t* dst, const application::ProductState& state, const application::ProductView& view, bool clockSet, uint16_t minutes,MotionFrame motion) {
  const auto& theme=assets::characterTheme(state.companion.selected);
  const auto* record = view.routine >= 0 && view.routine < state.count ? &state.routines[size_t(view.routine)] : nullptr;
  if(motion.kind==MotionKind::Letter&&motion.character<10&&motion.stage>=1&&motion.stage<=3&&motion.frame<21){
    const auto& movingTheme=assets::characterTheme(motion.character);image(dst,*movingTheme.background);
    const auto* letter=assets::letterFrames[assets::characterLetter[motion.character]][motion.frame<6?0:motion.frame-6];
    if(motion.frame<6){
      const auto* art=assets::homeCharacterArt[motion.character*3+motion.stage-1];
      drawImageAt(dst,*art,art->x,art->y,art->width,art->height,uint8_t(255*(5-motion.frame)/5));
      const int width=44+(int(letter->width)-44)*motion.frame/5,height=width*letter->height/letter->width;
      const int x=192-(192-(int(letter->x)+letter->width/2))*motion.frame/5;
      const int bottom=104+(320-104)*motion.frame/5;
      drawImageAt(dst,*letter,x-width/2,bottom-height,width,height);
    }else image(dst,*letter);
    image(dst,*movingTheme.topbar);clock(dst,clockSet,minutes);return;
  }
  if(motion.kind==MotionKind::Eating&&motion.character<10&&motion.stage>=1&&motion.stage<=3&&motion.food>=1&&motion.food<=31){
    renderHome(dst,{static_cast<Theme>(motion.character),0,clockSet,minutes,false,motion.stage});
    greeting(dst,"맛있게 먹을게~");const auto& bites=assets::biteClips[motion.food-1];
    if(!motion.frame)image(dst,*assets::rewardFoodArt[motion.food-1]);
    else if(motion.frame<=bites.count)image(dst,*bites.frames[motion.frame-1]);
    return;
  }
  // These backgrounds do not include a clock; draw it exactly once after the panel.
  switch (view.screen) {
    case Screen::Routine:
    case Screen::CompleteConfirm:
    case Screen::AbandonConfirm:
      renderRoutine(dst, theme, state, view, record,motion);
      image(dst,*theme.topbar);
      clock(dst, clockSet, minutes);
      return;
    case Screen::SummaryIntro:
    case Screen::Summary:
    case Screen::SummaryEnd:
      renderSummary(dst,state,view,motion);
      image(dst,*theme.topbar); clock(dst,clockSet,minutes); return;
    case Screen::Letter:
      image(dst,*theme.background);
      image(dst,*assets::homeCharacterArt[state.companion.selected*3+domain::growthView(state.companion.experience[state.companion.selected]).stage-1]);
      image(dst,assets::letterNotification);
      image(dst,*theme.topbar);
      clock(dst, clockSet, minutes);
      return;
    case Screen::Catalog:
      renderCatalogChoice(dst, theme, state, view);
      image(dst,*theme.topbar);
      clock(dst, clockSet, minutes);
      return;
    case Screen::CharacterCatalog:
    case Screen::CharacterConfirm:
      renderCharacterCatalog(dst,state,view);
      clock(dst,clockSet,minutes);
      return;
    case Screen::RewardCatalog:
    case Screen::FoodDetail:
      renderCatalogPage(dst,state,view);
      clock(dst,clockSet,minutes);
      return;
    default: break;
  }
  const auto character = state.companion.selected;
  const auto growth=domain::growthView(state.companion.experience[character]);
  renderHome(dst, {static_cast<Theme>(character), uint8_t(100*growth.progress/growth.target), clockSet, minutes, view.screen==Screen::Home,growth.stage});
  switch (view.screen) {
    case Screen::Home: break;
    case Screen::StepSaved:
    case Screen::Feed: renderReward(dst, theme, record, view); break;
    case Screen::Growth:
    case Screen::Evolution: renderGrowth(dst, theme, state, view); break;
    case Screen::NewCharacter: renderNewCharacter(dst, theme, view); break;
    case Screen::SummaryIntro:
    case Screen::Summary:
    case Screen::SummaryEnd: break;
    default: renderMessage(dst, theme, record, view); break;
  }
}
}
