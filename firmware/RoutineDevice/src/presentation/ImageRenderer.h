#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\ImageRenderer.h"
#pragma once
#include <cstddef>
#include <cstdint>

namespace assets {
// Cropped RGB565 + alpha, encoded by tools/image_lzss.py. Coordinates retain
// the Figma slot; transparent pixels outside the crop need no flash storage.
struct CompressedImage {
  const uint8_t* bytes;
  size_t size;
  uint16_t x, y, width, height;
  uint32_t archiveOffset = 0xffffffffu;
};
}
namespace routine::ui {
// Uses one 4KiB static window on the single UI loop. No decoder allocation.
// The archive provider may read a bounded compressed sprite from its file cache.
// Returns false for malformed data; every source and destination access is bounded.
bool drawImage(uint16_t* dst, const assets::CompressedImage& image);
// Reuse a cropped sprite at a bounded destination; suitable for transitions.
// Nearest sample of the source's antialiased pixels, preserving alpha.
bool drawImageAt(uint16_t* dst,const assets::CompressedImage& image,int x,int y,int width,int height,uint8_t opacity=255);
}
