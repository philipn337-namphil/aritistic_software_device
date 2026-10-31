#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\domain\\Companion.h"
#pragma once
#include <array>
#include <cstdint>

namespace routine::domain {
constexpr uint8_t kMaxCharacters = 50;
constexpr uint8_t kNoCharacter = 255;
struct GrowthPolicy {
  // Figma 106:1930, 102:2782, 322:2413: each stage has its own denominator.
  std::array<uint16_t,3> stageTargets{40,50,60};
  uint16_t total() const { return stageTargets[0]+stageTargets[1]+stageTargets[2]; }
};
inline constexpr GrowthPolicy kFigmaGrowth{};
enum class GrowthNotice : uint8_t { None, Evolved, Completed };
// Demo default per Figma "디바이스 화면 전체 정리(최종최종)": sky-blue polar bear (8),
// drawn at its third (standing, waving) stage. Only fresh state uses this;
// saved progress keeps its own selection and growth.
constexpr uint8_t kDefaultCharacter = 8;
constexpr uint8_t kDefaultStage = 3;
template <typename T>
constexpr std::array<T,kMaxCharacters> defaultEntry(T value) {
  std::array<T,kMaxCharacters> entries{};
  entries[kDefaultCharacter] = value;
  return entries;
}
struct Companion {
  uint64_t owned = uint64_t(1) << kDefaultCharacter;
  // Start of stage 3: the first two stage targets are already filled.
  std::array<uint16_t,kMaxCharacters> experience =
      defaultEntry<uint16_t>(uint16_t(kFigmaGrowth.stageTargets[0] + kFigmaGrowth.stageTargets[1]));
  // Highest collected stage survives restarting a completed companion.
  std::array<uint8_t,kMaxCharacters> highestStage = defaultEntry<uint8_t>(kDefaultStage);
  uint8_t selected = kDefaultCharacter;
  uint8_t offered = kNoCharacter;
  GrowthNotice notice = GrowthNotice::None;
};
struct GrowthView { uint8_t stage; uint16_t progress; uint16_t target; bool complete; };
inline GrowthView growthView(uint16_t experience, GrowthPolicy policy = kFigmaGrowth) {
  for (uint8_t i=0;i<3;++i) {
    if (experience < policy.stageTargets[i]) return {uint8_t(i+1),experience,policy.stageTargets[i],false};
    if (i == 2) return {3,policy.stageTargets[i],policy.stageTargets[i],true};
    experience -= policy.stageTargets[i];
  }
  return {};
}
inline bool owns(const Companion& state, uint8_t character) {
  return character < kMaxCharacters && (state.owned & (uint64_t(1)<<character));
}
inline uint8_t collectedStage(const Companion& state, uint8_t character) {
  if (!owns(state,character)) return 0;
  const auto current = growthView(state.experience[character]).stage;
  return state.highestStage[character] > current ? state.highestStage[character] : current;
}
inline bool collected(const Companion& state, uint8_t character, uint8_t stage) {
  return stage >= 1 && stage <= 3 && collectedStage(state,character) >= stage;
}
inline bool validCompanion(const Companion& state, uint8_t contentCount) {
  if (!contentCount || contentCount > kMaxCharacters || !state.owned ||
      (state.owned >> contentCount) || !owns(state,state.selected) ||
      unsigned(state.notice)>unsigned(GrowthNotice::Completed)) return false;
  if (state.offered != kNoCharacter && (state.offered >= contentCount || owns(state,state.offered))) return false;
  for (uint8_t i=0;i<kMaxCharacters;++i)
    if (state.experience[i]>kFigmaGrowth.total() || state.highestStage[i]>3 ||
        (!owns(state,i) && (state.experience[i] || state.highestStage[i]))) return false;
  const auto experience=state.experience[state.selected];
  if (state.notice==GrowthNotice::Evolved && experience!=kFigmaGrowth.stageTargets[0] &&
      experience!=kFigmaGrowth.stageTargets[0]+kFigmaGrowth.stageTargets[1]) return false;
  if ((state.notice==GrowthNotice::Completed || state.offered!=kNoCharacter) && experience!=kFigmaGrowth.total()) return false;
  return true;
}
// The caller saves this with the consumed entitlement. Animation never grants XP.
inline bool feedCompanion(Companion& state, uint8_t contentCount) {
  if (!validCompanion(state,contentCount) || state.notice!=GrowthNotice::None || state.offered!=kNoCharacter) return false;
  auto& experience=state.experience[state.selected];
  if (experience >= kFigmaGrowth.total()) return false;
  const auto previous=growthView(experience);
  ++experience;
  const auto next=growthView(experience);
  state.highestStage[state.selected]=collectedStage(state,state.selected);
  if (next.complete) {
    state.notice=GrowthNotice::Completed;
    // Deterministic content order until the designer supplies unlock probabilities.
    for (uint8_t i=0;i<contentCount;++i) if (!owns(state,i)) { state.offered=i; break; }
  } else if (next.stage!=previous.stage) state.notice=GrowthNotice::Evolved;
  return true;
}
}
