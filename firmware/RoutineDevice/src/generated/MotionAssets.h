#pragma once
#include "Assets.h"
namespace assets {
struct SpritePlacement { int16_t x,y,w,h; };
struct CryClip { const CompressedImage* body; SpritePlacement tears[5]; };
struct BiteClip { uint8_t count; const CompressedImage* frames[3]; };
extern const CryClip cryClips[30];
extern const BiteClip biteClips[31];
extern const CompressedImage tearSprite;
// Figma "편지모션": one sheet per species; frame 0 sits at the speech bubble.
inline constexpr uint8_t kLetterFrames = 14;
extern const CompressedImage* letterFrames[10][kLetterFrames];
// Figma "기본 모션": two breathing poses per species stage (nullptr when not drawn).
extern const CompressedImage* idleFrames[30][2];
}
