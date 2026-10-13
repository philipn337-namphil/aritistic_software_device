#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\Drawing.cpp"
#include "Drawing.h"
#include "HomeRenderer.h"
#include "FontBitmap.h"
#include "../generated/ProductAssets.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace routine::ui::drawing {
namespace {
void pixel(uint16_t* dst, int x, int y, uint16_t color, unsigned a) {
  if (x < 0 || x >= kWidth || y < 0 || y >= kHeight || !a) return;
  auto& d = dst[y * kWidth + x];
  d = uint16_t(((((color >> 11)*a + (d >> 11)*(255-a) + 127)/255) << 11) |
      (((((color >> 5)&63)*a + ((d >> 5)&63)*(255-a) + 127)/255) << 5) |
      (((color&31)*a + (d&31)*(255-a) + 127)/255));
}
}
void rect(uint16_t* dst, int x, int y, int w, int h, uint16_t color, unsigned alpha, int radius) {
  for (int yy = 0; yy < h; ++yy) for (int xx = 0; xx < w; ++xx) {
    if (radius) {
      const int dx = xx < radius ? radius-xx-1 : (xx >= w-radius ? xx-(w-radius) : 0);
      const int dy = yy < radius ? radius-yy-1 : (yy >= h-radius ? yy-(h-radius) : 0);
      if (dx*dx + dy*dy > radius*radius) continue;
    }
    pixel(dst,x+xx,y+yy,color,alpha);
  }
}
void image(uint16_t* dst, const assets::CompressedImage& image) {
  drawImage(dst,image);
}
namespace {
uint32_t codepoint(const char*& p) {
  const auto first = uint8_t(*p++); if (first < 128) return first;
  unsigned n = first < 0xe0 ? 1 : first < 0xf0 ? 2 : 3;
  uint32_t cp = first & (n == 1 ? 31 : n == 2 ? 15 : 7);
  while (n-- && *p) cp = (cp << 6) | (uint8_t(*p++) & 63);
  return cp;
}
const assets::Glyph* glyph(const assets::Font& font, uint32_t cp) {
  size_t lo = 0, hi = font.count;
  while (lo < hi) { const size_t m = (lo+hi)/2; if (font.glyphs[m].codepoint < cp) lo = m+1; else hi = m; }
  if (lo < font.count && font.glyphs[lo].codepoint == cp) return &font.glyphs[lo];
  return cp != '?' ? glyph(font,'?') : nullptr;
}
bool supports(const assets::Font& font, const char* p) {
  while (*p) { const auto cp = codepoint(p); const auto* g = glyph(font,cp); if (!g || g->codepoint != cp) return false; }
  return true;
}
int advance(uint32_t cp, int size) { const auto* g = glyph(assets::koreanFont,cp); return g ? (g->advance64*size + 704)/1408 : 0; }
}
int width(const char* p, int size) { int w = 0; while (*p) w += advance(codepoint(p),size); return w; }
namespace {
void drawText(uint16_t* dst, const assets::Font& font, const char* p, int center,
              int baseline, int size, int source, uint16_t color, bool centered, bool smooth = false) {
  int w = 0; const char* measure = p;
  while (*measure) { const auto* g = glyph(font,codepoint(measure)); if (g) w += (g->advance64*size + source*32)/(source*64); }
  int x = centered ? center-w/2 : center;
  while (*p) {
    const auto* g = glyph(font,codepoint(p)); if (!g) continue;
    uint8_t scratch[128];
    const auto* bitmap=fontBitmap(font,size_t(g-font.glyphs),scratch,sizeof(scratch));
    if(!bitmap){x+=(g->advance64*size+source*32)/(source*64);continue;}
    const int gw = (g->width*size + source-1)/source, gh = (g->height*size + source-1)/source;
    for (int yy = 0; yy < gh; ++yy) for (int xx = 0; xx < gw; ++xx) {
      const size_t offset = size_t(yy*source/size)*g->width + xx*source/size;
      unsigned alpha = font.bitsPerPixel==8 ? bitmap[offset] :
        ((bitmap[offset/4] >> (6-2*(offset%4)))&3)*85;
      if(smooth && size!=source) {
        const auto coverage=[&](int sx,int sy)->float {
          if(sx<0||sy<0||sx>=g->width||sy>=g->height)return 0;
          const size_t at=size_t(sy)*g->width+sx;
          return font.bitsPerPixel==8 ? bitmap[at] :
              ((bitmap[at/4]>>(6-2*(at%4)))&3)*85;
        };
        const float sx=(xx+0.5f)*source/size-0.5f,sy=(yy+0.5f)*source/size-0.5f;
        const int ix=int(std::floor(sx)),iy=int(std::floor(sy));
        const float fx=sx-ix,fy=sy-iy;
        alpha=unsigned((coverage(ix,iy)*(1-fx)+coverage(ix+1,iy)*fx)*(1-fy)+
                       (coverage(ix,iy+1)*(1-fx)+coverage(ix+1,iy+1)*fx)*fy+0.5f);
      }
      pixel(dst,x+g->left*size/source+xx,baseline+g->top*size/source+yy,color,alpha);
    }
    x += (g->advance64*size + source*32)/(source*64);
  }
}
}
void smoothText(uint16_t* dst,const char* p,int center,int baseline,int size,uint16_t color,bool centered) {
  drawText(dst,assets::koreanFont,p,center,baseline,size,22,color,centered,true);
}
void roundedRect(uint16_t* dst, int x, int y, int w, int h, uint16_t color, float radius) {
  radius=std::max(0.0f,std::min(radius,std::min(w,h)*0.5f));
  for (int yy=0; yy<h; ++yy) for (int xx=0; xx<w; ++xx) {
    const float dx=std::max(0.0f,std::abs(xx+0.5f-w*0.5f)-(w*0.5f-radius));
    const float dy=std::max(0.0f,std::abs(yy+0.5f-h*0.5f)-(h*0.5f-radius));
    const float coverage=(dx && dy) ? std::max(0.0f,std::min(1.0f,radius+0.5f-std::sqrt(dx*dx+dy*dy))) : 1.0f;
    pixel(dst,x+xx,y+yy,color,unsigned(coverage*255+0.5f));
  }
}
int width(const assets::Font& font, const char* p) {
  int w = 0;
  while (*p) { const auto* g = glyph(font,codepoint(p)); if (g) w += (g->advance64+32)/64; }
  return w;
}
void text(uint16_t* dst, const assets::Font& font, const char* p, int center, int baseline, uint16_t color, bool centered) {
  drawText(dst,font,p,center,baseline,1,1,color,centered);
}
void text(uint16_t* dst, const char* p, int center, int baseline, int size, uint16_t color, bool centered) {
  const assets::Font* native = nullptr;
  if (size == 14 && supports(assets::statusFont,p)) native = &assets::statusFont;
  else if (size == 16 && supports(assets::timeFont,p)) native = &assets::timeFont;
  else if (size == 18 && supports(assets::growthFont,p)) native = &assets::growthFont;
  else if (size == 20 && supports(assets::uiFont,p)) native = &assets::uiFont;
  drawText(dst,native ? *native : assets::koreanFont,p,center,baseline,size,native ? size : 22,color,centered);
}
void block(uint16_t* dst, const char* p, int top, int maxSize, int rows, uint16_t color) {
  int size = maxSize;
  while (size > 10 && width(p,size) > 184*rows) --size;
  char line[128]{}; size_t length = 0; int used = 0, row = 0;
  while (*p && row < rows) {
    const char* start = p; const int nextWidth = advance(codepoint(p),size); const size_t bytes = size_t(p-start);
    if (used+nextWidth > 190 && length) {
      line[length] = 0; text(dst,line,120,top+size+row*(size+4),size,color); ++row; length = 0; used = 0;
      if (row == rows) break;
    }
    if (length+bytes >= sizeof(line)) break;
    std::memcpy(line+length,start,bytes); length += bytes; used += nextWidth;
  }
  if (length && row < rows) { line[length] = 0; text(dst,line,120,top+size+row*(size+4),size,color); }
}
}
