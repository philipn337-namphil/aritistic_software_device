#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\CatalogRenderer.cpp"
#include "CatalogRenderer.h"
#include "Drawing.h"
#include "HomeRenderer.h"
#include "../content/Catalog.h"
#include "../generated/CatalogAssets.h"
#include "../generated/ThemeAssets.h"
#include <algorithm>
#include <cstdio>

namespace routine::ui {
namespace {
constexpr uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return uint16_t(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
}
void renderCatalogPage(uint16_t* dst, const application::ProductState& state,
                       const application::ProductView& view) {
  using application::Screen;
  using namespace drawing;
  const bool character=view.screen==Screen::CharacterCatalog || view.screen==Screen::CharacterConfirm;
  const auto count=character ? content::kCharacterEntryCount : content::kFoodCount;
  const bool close=view.item>=count;
  const auto& theme=assets::characterTheme(state.companion.selected);
  const uint16_t panel=theme.panel,button=theme.button;
  const auto entry=content::characterEntry(view.item,state.companion.selected);
  const auto inventory=application::foodInventory(state,uint8_t(view.item+1));
  const bool acquired=!close && (character ? domain::collected(state.companion,entry.character,entry.stage)
                                           : inventory.acquired!=0 || content::kShowcase);
  std::fill_n(dst,kWidth*kHeight,uint16_t(0xffff));
  image(dst,*theme.topbar);
  text(dst,assets::catalogHeadingFont,"도감",39,51,0);
  if (!close) {
    char position[24];
    std::snprintf(position,sizeof(position),"%u/%u",unsigned(view.item+1),unsigned(character ? content::kCatalogTarget : count));
    text(dst,assets::catalogHeadingFont,position,
         character ? acquired ? 209 : 204 : 221-width(assets::catalogHeadingFont,position),51,0,character);
  }
  // Only the top corners are rounded in Figma; the card reaches the LCD bottom.
  roundedRect(dst,8,62,224,258,panel,14);
  rect(dst,8,306,224,14,panel);
  const assets::CatalogArt* art=nullptr;
  if (!close) {
    if (character) {
      const auto index=entry.character*3+entry.stage-1;
      art=&(acquired ? assets::characterArt[index] : assets::lockedCharacterArt[index]);
    } else if (acquired) art=&assets::foodArt[view.item];
    if (art && art->image) image(dst,*art->image);
    else if (character) text(dst,assets::catalogFont,"???",120,195,button);
  }
  const char* name=close ? "홈으로" : acquired ? character ? content::characterName(entry.character) : content::foodName(uint8_t(view.item+1)) : "???";
  const bool centered=art==nullptr || art->nameX==120;
  text(dst,assets::catalogFont,name,art ? art->nameX : 120,character?88:102,0,centered);
  if (!character && !close && !acquired) text(dst,assets::catalogFont,"???",120,210,button);
  image(dst,*theme.arrows);
  if (view.screen==Screen::FoodDetail && !close) {
    char detail[48];
    std::snprintf(detail,sizeof(detail),"획득 %u개 / 남은 %u개",unsigned(inventory.acquired),unsigned(inventory.available));
    rect(dst,35,248,170,20,panel);
    text(dst,assets::catalogDetailFont,detail,120,265,0);
  }
  // Figma 최종최종: both catalogues use the same short "선택" pill.
  roundedRect(dst,75,269,90,39,button,19.5f);
  if (close || acquired) {
    const char* action=close ? "확인" : character ? "선택" :
      view.screen==Screen::FoodDetail && !inventory.available ? "확인" : "선택";
    text(dst,assets::catalogFont,action,121,296,0xffff);
  } else image(dst,assets::catalogLock);
  if (view.screen==Screen::CharacterConfirm && !close) {
    // The source catalog confirmation has no blur/veil. Its older placeholder
    // character is replaced by the actual selected catalog stage above.
    rect(dst,0,199,240,121,button);
    const char* prompt=state.companion.experience[entry.character]==domain::kFigmaGrowth.total()
        ? "다시 키우겠습니까?" : "함께 지낼까요?";
    text(dst,assets::catalogPromptFont,prompt,121,237,rgb(255,245,230));
    roundedRect(dst,24,254,86,39,rgb(47,47,47),19.5f);
    roundedRect(dst,131,254,86,39,rgb(47,47,47),19.5f);
    text(dst,assets::catalogFont,view.selected==0?">취소":"취소",67,281,0xffff);
    text(dst,assets::catalogFont,view.selected==1?">확인":"확인",174,281,0xffff);
  }
}
}
