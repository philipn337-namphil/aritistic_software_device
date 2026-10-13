#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\generated\\MotionAssets.h"
#pragma once
#include "Assets.h"
namespace assets {
struct SpritePlacement { int16_t x,y,w,h; };
struct CryClip { const CompressedImage* body; SpritePlacement tears[5]; };
struct BiteClip { uint8_t count; const CompressedImage* frames[3]; };
extern const CryClip cryClips[30];
extern const BiteClip biteClips[31];
extern const CompressedImage tearSprite;
extern const CompressedImage* letterFrames[9][15];
extern const uint8_t characterLetter[10];
}
