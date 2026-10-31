#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\content\\Catalog.h"
#pragma once
#include <cstdint>

// Showcase builds (simulator and web) open every character stage and food in
// the catalogue. The real device keeps earned-only collection to fit its flash.
#ifndef ROUTINE_SHOWCASE
#define ROUTINE_SHOWCASE 0
#endif
namespace routine::content {
inline constexpr bool kShowcase = ROUTINE_SHOWCASE != 0;
// Local content IDs, independent of server IDs. Extend with verified source art.
constexpr uint8_t kFoodCount = 31;
constexpr uint8_t kCharacterCount = 10;
constexpr uint8_t kCharacterEntryCount = kCharacterCount * 3;
// User decision 2026-10-04 supersedes Figma's placeholder /50.
constexpr uint8_t kCatalogTarget = kCharacterEntryCount;
struct CharacterEntry { uint8_t character, stage; };
// Figma "캐릭터 도감" (최종최종) species order: polar bear, sheep, goat, cat,
// dog, rabbit, seal, red robot, green robot, blue robot (character IDs).
inline constexpr uint8_t kCatalogOrder[kCharacterCount] = {8,0,2,4,5,9,6,1,3,7};
// The current companion's three stages first, then the rest in Figma order.
inline CharacterEntry characterEntry(uint8_t index, uint8_t selected) {
  if (index >= kCharacterEntryCount || selected >= kCharacterCount) return CharacterEntry{255,0};
  uint8_t start = 0;
  while (start < kCharacterCount && kCatalogOrder[start] != selected) ++start;
  return CharacterEntry{kCatalogOrder[(start % kCharacterCount + index/3) % kCharacterCount],uint8_t(index%3+1)};
}
const char* foodName(uint8_t id); // 1..31, zero is unassigned.
const char* characterName(uint8_t id); // 0..count-1.
uint8_t drawFood(uint32_t& randomState);
}
