#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\Drawing.h"
#pragma once
#include <cstdint>

namespace assets { struct CompressedImage; struct Font; }
namespace routine::ui::drawing {
// Drawing operations target the shared 240x320 RGB565 framebuffer.
// They know no product state and do not read or write storage.
void rect(uint16_t* dst, int x, int y, int w, int h, uint16_t color,
          unsigned alpha = 255, int radius = 0);
void roundedRect(uint16_t* dst, int x, int y, int w, int h, uint16_t color, float radius);
void image(uint16_t* dst, const assets::CompressedImage& image);
int width(const char* text, int size);
void text(uint16_t* dst, const char* text, int center, int baseline, int size,
          uint16_t color, bool centered = true);
// Interpolate coverage when arbitrary server text uses a non-native font size.
void smoothText(uint16_t* dst, const char* text, int center, int baseline, int size,
                uint16_t color, bool centered = true);
// Explicit native-size font for design-specific labels; uses coverage masks.
int width(const assets::Font& font, const char* text);
void text(uint16_t* dst, const assets::Font& font, const char* text, int center,
          int baseline, uint16_t color, bool centered = true);
void block(uint16_t* dst, const char* text, int top, int maxSize, int rows, uint16_t color = 0);
}
