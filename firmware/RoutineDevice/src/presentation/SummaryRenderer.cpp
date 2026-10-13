#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\SummaryRenderer.cpp"
#include "SummaryRenderer.h"
#include "Drawing.h"
#include "../generated/SummaryAssets.h"
#include "../generated/ThemeAssets.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace routine::ui {
namespace {
using namespace drawing;
using application::Screen;
void moveImage(uint16_t* dst,const assets::CompressedImage& art,int dx,int dy) {
  drawImageAt(dst,art,art.x+dx,art.y+dy,art.width,art.height);
}
// Server titles can be much longer than the Figma sample. Keep marks/controls
// visible, shrink within readable limits, then truncate only at UTF-8 boundaries.
void label(uint16_t* dst,const char* value,int x,int baseline,int maxWidth,bool centered) {
  int size=24;
  while(size>16 && width(value,size)>maxWidth) --size;
  char visible[application::kTextBytes+20];
  std::snprintf(visible,sizeof(visible),"%s",value);
  if(width(visible,size)>maxWidth) {
    size_t n=std::strlen(visible);
    do {
      if(!n) break;
      --n;while(n && (uint8_t(visible[n])&0xc0)==0x80) --n;
      std::memcpy(visible+n,"...",4);
    } while(width(visible,size)>maxWidth);
  }
  smoothText(dst,visible,x,baseline,size,0,centered);
}
void title(uint16_t* dst,const application::RoutineRecord& record,int center=121,int top=65) {
  const auto minutes=unsigned((record.run.spec.startsAt%86400+9*3600)%86400/60);
  char time[8],name[application::kTextBytes+20];
  std::snprintf(time,sizeof(time),"%u:%02u",minutes/60,minutes%60);
  text(dst,assets::summaryTimeFont,time,center,top+25,0);
  std::snprintf(name,sizeof(name),"%s 시작!",record.title.data());
  label(dst,name,center,top+58,190,true);
}
void greeting(uint16_t* dst,const application::ProductState& state,bool end) {
  const auto character=state.companion.selected;
  const auto& theme=assets::characterTheme(character);
  const auto growth=domain::growthView(state.companion.experience[character]);
  image(dst,*assets::homeCharacterArt[character*3+growth.stage-1]);
  image(dst,*(end?assets::summaryEnd[character]:assets::summaryIntro[character]));
  if(end) {
    text(dst,assets::summaryLabelFont,"오늘 하루도 수고했어~",124,63,0);
    text(dst,assets::summaryLabelFont,"내일 봐~",124,90,0);
  } else text(dst,assets::summaryLabelFont,"오늘 하루를 요약해줄게~",121,78,0);
  roundedRect(dst,45,291,157,8,theme.track,2);
  roundedRect(dst,39,291,163*growth.progress/growth.target,8,theme.accent,2);
}
void letter(uint16_t* dst,const assets::CharacterTheme& theme,
            const application::RoutineRecord& record,uint8_t frame) {
  // Six static drawings 102:3092..102:3157 form the documented opening sequence.
  // No invented easing/keyframes; presentation duration is in MotionTiming.
  if(!frame){image(dst,assets::summaryClosed);return;}
  constexpr int backOffset[]={0,0,0,22,45,84};
  constexpr int paperY[]={0,140,109,63,52,52};
  constexpr int paperHeight[]={0,84,131,199,229,229};
  constexpr int frontOffset[]={0,0,9,31,49,92};
  if(frame==1) image(dst,assets::summaryBackShort);
  else moveImage(dst,assets::summaryBack,0,backOffset[frame]);
  rect(dst,20,paperY[frame],202,paperHeight[frame],theme.panel);
  if(frame>=3) title(dst,record,frame==3?129:121,frame==3?63:65);
  moveImage(dst,assets::summaryFront,frame>=4?2:0,frontOffset[frame]);
}
}
void renderSummary(uint16_t* dst,const application::ProductState& state,
                   const application::ProductView& view,MotionFrame motion) {
  const auto& theme=assets::characterTheme(state.companion.selected);
  image(dst,*theme.background);
  if(view.screen!=Screen::Summary) {
    greeting(dst,state,view.screen==Screen::SummaryEnd);return;
  }
  const auto page=application::summaryPage(state,view.summary.day,view.item);
  if(!page.valid()) return;
  const auto& record=state.routines[size_t(page.routine)];
  if(motion.kind==MotionKind::SummaryLetter && motion.frame<6) {
    letter(dst,theme,record,motion.frame);return;
  }
  rect(dst,20,52,202,229,theme.panel);
  if(domain::terminal(record.run.phase) && record.run.phase!=domain::Phase::Completed)
    image(dst,assets::summaryUnfinished);
  title(dst,record);
  const assets::CompressedImage* checks[]={&assets::summaryCheck0,&assets::summaryCheck1,&assets::summaryCheck2};
  const auto completed=domain::completedMask(record.run);
  for(uint8_t row=0;row<page.stepCount;++row) {
    const auto step=uint8_t(page.firstStep+row);
    const bool done=completed&(1u<<step);
    label(dst,record.stepNames[step].data(),45,181+row*34,done?117:151,false);
    if(done) image(dst,*checks[row]);
  }
  image(dst,assets::summaryArrows);
  // Same Figma exit affordance on every sheet, so confirmation is discoverable
  // without first reaching the last routine. Arrows only browse the daily sheets.
  roundedRect(dst,136,272,86,39,0x2965,19.5f);
  text(dst,assets::summaryLabelFont,"나가기",179,299,0xffff);
}
}
