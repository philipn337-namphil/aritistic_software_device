#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\FontBitmap.h"
#pragma once
#include "../generated/Assets.h"
namespace routine::ui {
// One independently compressed glyph, in the same LZSS format as image assets.
// The caller supplies a small bounded buffer; no font-sized allocation is needed.
const uint8_t* fontBitmap(const assets::Font& font,size_t index,uint8_t* scratch,size_t capacity);
}
