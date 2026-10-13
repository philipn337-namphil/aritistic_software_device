#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\content\\Catalog.h"
#pragma once
#include <cstdint>

namespace routine::content {
// Local content IDs, independent of server IDs. Extend with verified source art.
constexpr uint8_t kFoodCount = 31;
constexpr uint8_t kCharacterCount = 10;
constexpr uint8_t kCharacterEntryCount = kCharacterCount * 3;
// User decision 2026-10-04 supersedes Figma's placeholder /50.
constexpr uint8_t kCatalogTarget = kCharacterEntryCount;
struct CharacterEntry { uint8_t character, stage; };
// Current companion's three stages first, then other supplied species.
inline CharacterEntry characterEntry(uint8_t index, uint8_t selected) {
  return index < kCharacterEntryCount && selected < kCharacterCount
      ? CharacterEntry{uint8_t((selected+index/3)%kCharacterCount),uint8_t(index%3+1)}
      : CharacterEntry{255,0};
}
const char* foodName(uint8_t id); // 1..31, zero is unassigned.
const char* characterName(uint8_t id); // 0..count-1.
uint8_t drawFood(uint32_t& randomState);
}
