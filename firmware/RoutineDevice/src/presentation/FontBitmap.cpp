#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\FontBitmap.cpp"
#include "FontBitmap.h"

namespace routine::ui {
const uint8_t* fontBitmap(const assets::Font& font,size_t index,uint8_t* scratch,size_t capacity){
  if(!font.pixels||!font.glyphs||index>=font.count)return nullptr;
  const auto& glyph=font.glyphs[index];
  if(!font.compressed)return font.pixels+glyph.offset;
  if(font.bitsPerPixel!=2||!scratch)return nullptr;
  const size_t size=(size_t(glyph.width)*glyph.height+3)/4;
  const size_t end=index+1<font.count?font.glyphs[index+1].offset:font.pixelBytes;
  if(!size||size>capacity||glyph.offset>=end||end>font.pixelBytes)return nullptr;
  size_t at=glyph.offset,produced=0;
  while(produced<size){
    if(at==end)return nullptr;
    const uint8_t flags=font.pixels[at++];
    for(unsigned bit=0;bit<8&&produced<size;++bit){
      if(flags&(1u<<bit)){
        if(end-at<2)return nullptr;
        const unsigned token=font.pixels[at]|(unsigned(font.pixels[at+1])<<8);at+=2;
        const unsigned distance=(token&4095)+1;unsigned count=(token>>12)+3;
        if(count==18){if(at==end)return nullptr;count+=font.pixels[at++];}
        if(distance>produced||count>size-produced)return nullptr;
        while(count--){scratch[produced]=scratch[produced-distance];++produced;}
      }else{if(at==end)return nullptr;scratch[produced++]=font.pixels[at++];}
    }
  }
  return at==end?scratch:nullptr;
}
}
