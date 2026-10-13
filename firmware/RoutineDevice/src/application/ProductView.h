#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\ProductView.h"
#pragma once
#include "DailySummary.h"

namespace routine::application {
// Screen values also appear in the USB status output; keep existing values stable.
enum class Screen {
  Home, Letter, Routine, CompleteConfirm, AbandonConfirm, StepSaved, Result,
  Catalog, CharacterCatalog, RewardCatalog, ClockRequired, StorageError,
  Feed, Growth, Evolution, NewCharacter, CharacterConfirm, FoodDetail,
  SummaryIntro, Summary, SummaryEnd
};
struct ProductView {
  Screen screen = Screen::Home;
  uint8_t selected = 1;
  int8_t routine = -1;
  uint32_t expectedVersion = 0;
  domain::StepId expectedStep = 0;
  bool clockKnown = false;
  bool warning = false;
  uint8_t item = 0;
  uint8_t rewardStep = 0;
  DailySummary summary{};
};
inline bool isSummaryScreen(Screen screen) {
  return screen == Screen::SummaryIntro || screen == Screen::Summary || screen == Screen::SummaryEnd;
}
inline bool sameInputTarget(const ProductView& before, const ProductView& after) {
  return before.screen == after.screen && before.routine == after.routine &&
      before.expectedVersion == after.expectedVersion &&
      before.expectedStep == after.expectedStep && before.selected == after.selected &&
      before.item == after.item && before.rewardStep == after.rewardStep &&
      (!isSummaryScreen(before.screen) || before.summary.day == after.summary.day);
}
}
