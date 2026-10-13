#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\ImageRenderer.cpp"
#include "ImageRenderer.h"
#include "HomeRenderer.h"
#include "ImageArchive.h"
#include <algorithm>

namespace routine::ui {
namespace {
uint8_t history[4096];
struct Decoder {
  const assets::CompressedImage& source;
  size_t at = 0, produced = 0;
  unsigned flags = 0, bits = 0, remaining = 0, distance = 0;
  bool byte(uint8_t& value) {
    if (!remaining) {
      if (!bits) {
        if (at == source.size) return false;
        flags = source.bytes[at++]; bits = 8;
      }
      const bool match = flags & 1;
      flags >>= 1; --bits;
      if (match) {
        if (source.size-at < 2) return false;
        const unsigned token = source.bytes[at] | (unsigned(source.bytes[at+1]) << 8);
        at += 2; distance = (token & 4095)+1; remaining = (token >> 12)+3;
        if (remaining == 18) {
          if (at == source.size) return false;
          remaining += source.bytes[at++];
        }
        if (distance > produced) return false;
      } else {
        if (at == source.size) return false;
        value = source.bytes[at++];
      }
    }
    if (remaining) { value = history[(produced-distance)&4095]; --remaining; }
    history[produced++&4095] = value;
    return true;
  }
};
uint16_t blend(uint16_t color, uint16_t background, unsigned a) {
  if (a == 255) return color;
  return uint16_t(((((color >> 11)*a + (background >> 11)*(255-a) + 127)/255) << 11) |
      (((((color >> 5)&63)*a + ((background >> 5)&63)*(255-a) + 127)/255) << 5) |
      (((color&31)*a + (background&31)*(255-a) + 127)/255));
}
}
bool drawImage(uint16_t* dst, const assets::CompressedImage& descriptor) {
  if(unsigned(descriptor.x)+descriptor.width>kWidth||unsigned(descriptor.y)+descriptor.height>kHeight)return false;
  return drawImageAt(dst,descriptor,descriptor.x,descriptor.y,descriptor.width,descriptor.height);
}
bool drawImageAt(uint16_t* dst,const assets::CompressedImage& descriptor,int left,int top,int width,int height,uint8_t opacity) {
  auto image=descriptor;
  if(!image.bytes && image.archiveOffset!=0xffffffffu) image.bytes=archivedImage(image.archiveOffset,image.size);
  if (!dst || !image.bytes || !image.size || !image.width || !image.height ||image.width>kWidth||image.height>kHeight||
      width<=0||height<=0||width>2*kWidth||height>2*kHeight||left < -2*kWidth||left>2*kWidth||top < -2*kHeight||top>2*kHeight) return false;
  Decoder decoder{image};
  for (unsigned y=0; y<image.height; ++y) for (unsigned x=0; x<image.width; ++x) {
    uint8_t low, high, alpha;
    if (!decoder.byte(low) || !decoder.byte(high) || !decoder.byte(alpha)) return false;
    alpha=uint8_t((unsigned(alpha)*opacity+127)/255);
    if (alpha) {
      const int x0=std::max(0,left+int((x*width+image.width-1)/image.width));
      const int x1=std::min(kWidth,left+int(((x+1)*width+image.width-1)/image.width));
      const int y0=std::max(0,top+int((y*height+image.height-1)/image.height));
      const int y1=std::min(kHeight,top+int(((y+1)*height+image.height-1)/image.height));
      for(int yy=y0;yy<y1;++yy)for(int xx=x0;xx<x1;++xx){
        auto& pixel=dst[yy*kWidth+xx];pixel=blend(uint16_t(low|(uint16_t(high)<<8)),pixel,alpha);
      }
    }
  }
  return !decoder.remaining && decoder.at == image.size;
}
}
