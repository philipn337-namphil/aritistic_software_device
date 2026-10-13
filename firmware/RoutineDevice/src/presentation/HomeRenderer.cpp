#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\HomeRenderer.cpp"
#include "HomeRenderer.h"
#include "../generated/Assets.h"
#include "../generated/ThemeAssets.h"
#include <cstdio>
#include <cstring>

namespace routine::ui {
namespace {
void pixel(uint16_t* dst, int x, int y, uint16_t color, uint8_t alpha = 255) {
  if (x < 0 || y < 0 || x >= kWidth || y >= kHeight || alpha == 0) return;
  uint16_t& d = dst[y*kWidth+x];
  if (alpha == 255) { d = color; return; }
  // Blend in channel-native precision so white glyph edges stay antialiased.
  const unsigned a = alpha, b = 255-a;
  const unsigned r = (((color >> 11)*a + (d >> 11)*b + 127)/255);
  const unsigned g = ((((color >> 5)&63)*a + ((d >> 5)&63)*b + 127)/255);
  const unsigned bl = (((color&31)*a + (d&31)*b + 127)/255);
  d = uint16_t((r << 11) | (g << 5) | bl);
}
uint32_t nextCodepoint(const char*& p) {
  const uint8_t a = static_cast<uint8_t>(*p++);
  if (a < 128) return a;
  if ((a & 0xE0) == 0xC0) { uint32_t v = (a&31) << 6; return v | (static_cast<uint8_t>(*p++)&63); }
  if ((a & 0xF0) == 0xE0) { uint32_t v = (a&15) << 12; v |= (static_cast<uint8_t>(*p++)&63) << 6; return v | (static_cast<uint8_t>(*p++)&63); }
  return '?';
}
const assets::Glyph* glyph(const assets::Font& f, uint32_t cp) {
  for (size_t i=0; i<f.count; ++i) if (f.glyphs[i].codepoint == cp) return &f.glyphs[i];
  return nullptr;
}
int textWidth64(const assets::Font& f, const char* p) {
  int width=0;
  while (*p) { const auto* g = glyph(f,nextCodepoint(p)); if (g) width += g->advance64; }
  return width;
}
void text(uint16_t* dst, const assets::Font& font, const char* p, int x64, int baseline, uint16_t color) {
  while (*p) {
    const auto* g = glyph(font,nextCodepoint(p));
    if (!g) continue;
    const int x = (x64+32)/64 + g->left;
    for (int yy=0; yy<g->height; ++yy) for (int xx=0; xx<g->width; ++xx)
      pixel(dst,x+xx,baseline+g->top+yy,color,font.pixels[g->offset+yy*g->width+xx]);
    x64 += g->advance64;
  }
}
void roundBar(uint16_t* dst,int x,int y,int w,int h,uint16_t color) {
  if (w <= 0) return;
  for (int yy=0; yy<h; ++yy) for (int xx=0; xx<w; ++xx) {
    // Two-pixel corner radius as in the 8px-high Figma gauge.
    if (w>=4 && (yy==0 || yy==h-1) && (xx==0 || xx==w-1)) continue;
    pixel(dst,x+xx,y+yy,color);
  }
}
}
void renderHome(uint16_t* dst, const HomeViewModel& state) {
  const auto& theme=assets::characterTheme(uint8_t(state.theme));
  const auto stage=state.stage>=1&&state.stage<=3?state.stage:2;
  drawImage(dst,*theme.background);drawImage(dst,*theme.homeBase);
  drawImage(dst,*assets::homeCharacterArt[(uint8_t(state.theme)<10?uint8_t(state.theme):0)*3+stage-1]);
  const char* greeting = "좋은 하루 보내~";
  text(dst,assets::greetingFont,greeting,120*64-textWidth64(assets::greetingFont,greeting)/2,75,theme.greeting);
  char clock[16];
  if (state.clockSet) {
    const unsigned hour24=(state.minutesOfDay/60)%24;
    std::snprintf(clock,sizeof(clock),"%s %u:%02u",hour24<12?"AM":"PM",hour24%12?hour24%12:12,state.minutesOfDay%60);
  } else std::snprintf(clock,sizeof(clock),"--:--");
  text(dst,assets::timeFont,clock,8*64,20,0xffff);
  if (state.showProgress) {
    roundBar(dst,45,291,157,8,theme.track);
    const int progress = state.progress>100 ? 100 : state.progress;
    roundBar(dst,39,291,(163*progress+50)/100,8,theme.accent);
  }
}
}
