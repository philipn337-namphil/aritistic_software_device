#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\ProductInput.cpp"
#include "ProductController.h"
#include "RewardLedger.h"
#include "../content/Catalog.h"

namespace routine::application {
using input::Intent;

void ProductController::input(Intent intent, domain::TimeSample time) {
  if (intent == Intent::None) return;
  const auto before = view_;
  tick(time);
  // A deadline or date change can replace the screen while processing this press.
  if (fault_ || !sameInputTarget(before, view_)) return;

  switch (view_.screen) {
    case Screen::Home: handleHomeInput(intent); break;
    case Screen::Catalog: handleMenuInput(intent); break;
    case Screen::CharacterCatalog:
    case Screen::RewardCatalog: handleCatalogInput(intent); break;
    case Screen::CharacterConfirm:
    case Screen::NewCharacter: handleCharacterInput(intent); break;
    case Screen::FoodDetail:
    case Screen::StepSaved:
    case Screen::Feed: handleRewardInput(intent); break;
    case Screen::Growth:
    case Screen::Evolution: handleGrowthInput(intent); break;
    case Screen::SummaryIntro:
    case Screen::Summary:
    case Screen::SummaryEnd: handleSummaryInput(intent); break;
    case Screen::Letter:
    case Screen::Routine:
    case Screen::CompleteConfirm:
    case Screen::AbandonConfirm:
    case Screen::Result: handleRoutineInput(intent, time); break;
    case Screen::ClockRequired:
      if (intent == Intent::Confirm && now_.trusted) reconcile();
      break;
    case Screen::StorageError: break;
  }
}

void ProductController::moveChoice(Intent intent) {
  view_.selected = intent == Intent::Previous ? 0 : 1;
  ++viewRevision_;
}

void ProductController::handleHomeInput(Intent intent) {
  if (intent != Intent::Confirm) show(Screen::Catalog);
  else if (state_.companion.notice != domain::GrowthNotice::None ||
           state_.companion.offered != domain::kNoCharacter) reconcile();
  else if (view_.summary.finished()) show(Screen::SummaryIntro);
  else reconcile();
}

void ProductController::handleMenuInput(Intent intent) {
  if (intent == Intent::Confirm) show(view_.selected ? Screen::RewardCatalog : Screen::CharacterCatalog);
  else moveChoice(intent);
}

void ProductController::handleCatalogInput(Intent intent) {
  const bool characters = view_.screen == Screen::CharacterCatalog;
  const uint8_t count = characters ? content::kCharacterEntryCount : content::kFoodCount;
  const auto entry = content::characterEntry(view_.item,state_.companion.selected);
  if (intent != Intent::Confirm) {
    // The last entry is the explicit Home action, included in both directions.
    view_.item = uint8_t((view_.item + (intent == Intent::Previous ? count : 1)) % (count + 1));
    ++viewRevision_;
  } else if (view_.item == count) show(Screen::Home);
  else if (characters && domain::collected(state_.companion,entry.character,entry.stage)) show(Screen::CharacterConfirm);
  else if (!characters && foodInventory(state_, uint8_t(view_.item + 1)).acquired) show(Screen::FoodDetail);
}

void ProductController::handleCharacterInput(Intent intent) {
  if (intent != Intent::Confirm) { moveChoice(intent); return; }
  if (view_.screen == Screen::NewCharacter) {
    if (!view_.selected) { show(Screen::Home); return; }
    const auto offered = state_.companion.offered;
    if (offered == domain::kNoCharacter) return;
    candidate_ = state_;
    candidate_.companion.owned |= uint64_t(1) << offered;
    candidate_.companion.selected = offered;
    candidate_.companion.highestStage[offered] = 1;
    candidate_.companion.offered = domain::kNoCharacter;
    if (commit()) show(Screen::Growth);
    return;
  }
  if (!view_.selected) {
    const auto item = view_.item;
    show(Screen::CharacterCatalog);
    view_.item = item;
    return;
  }
  const auto entry = content::characterEntry(view_.item,state_.companion.selected);
  if (!domain::collected(state_.companion,entry.character,entry.stage)) return;
  if (state_.companion.offered != domain::kNoCharacter) { show(Screen::NewCharacter); return; }
  candidate_ = state_;
  candidate_.companion.selected = entry.character;
  candidate_.companion.highestStage[entry.character] = domain::collectedStage(state_.companion,entry.character);
  auto& experience = candidate_.companion.experience[entry.character];
  if (experience == domain::kFigmaGrowth.total()) experience = 0;
  if (commit()) reconcile();
}

void ProductController::handleRewardInput(Intent intent) {
  if (view_.screen == Screen::FoodDetail) {
    if (intent == Intent::Confirm) {
      const auto reward = findPendingFood(state_, uint8_t(view_.item + 1));
      if (reward.found()) { presentReward(reward.routine, reward.step); return; }
    }
    const auto item = view_.item;
    show(Screen::RewardCatalog);
    view_.item = item;
    return;
  }
  if (intent != Intent::Confirm) return;
  if (view_.screen == Screen::Feed) { feedReward(); return; }
  if (view_.routine < 0) return;
  candidate_ = state_;
  candidate_.routines[size_t(view_.routine)].acknowledgedRewards |= uint16_t(1u << view_.rewardStep);
  if (commit()) reconcile();
}

void ProductController::handleGrowthInput(Intent intent) {
  if (intent != Intent::Confirm) return;
  if (view_.screen == Screen::Growth) { reconcile(); return; }
  candidate_ = state_;
  candidate_.companion.notice = domain::GrowthNotice::None;
  if (commit()) {
    show(state_.companion.offered != domain::kNoCharacter ? Screen::NewCharacter : Screen::Growth);
  }
}

void ProductController::showSummaryPage(uint8_t page) {
  const auto sheet = summaryPage(state_,view_.summary.day,page);
  if (!sheet.valid()) { reconcile(); return; }
  show(Screen::Summary,sheet.routine);
  view_.item = page;
}
void ProductController::handleSummaryInput(Intent intent) {
  switch (view_.screen) {
    case Screen::SummaryIntro:
      if (intent == Intent::Confirm || intent == Intent::Next) showSummaryPage(0);
      break;
    case Screen::Summary: {
      if (intent == Intent::Confirm) { show(Screen::SummaryEnd); break; }
      const auto sheet = summaryPage(state_,view_.summary.day,view_.item);
      if (!sheet.pages) { reconcile(); break; }
      showSummaryPage(uint8_t((view_.item + (intent == Intent::Previous ? sheet.pages-1 : 1)) % sheet.pages));
      break;
    }
    case Screen::SummaryEnd:
      if (intent != Intent::Confirm) { showSummaryPage(view_.item); return; }
      candidate_ = state_;
      candidate_.summarySeenDay = view_.summary.day;
      if (commit()) show(Screen::Home);
      break;
    default: break;
  }
}

void ProductController::handleRoutineInput(Intent intent, domain::TimeSample time) {
  if (view_.routine < 0) return;
  const size_t index = static_cast<size_t>(view_.routine);
  const bool confirm = intent == Intent::Confirm;
  if (view_.screen == Screen::Result) {
    if (!confirm) return;
    candidate_ = state_;
    candidate_.routines[index].resultSeen = true;
    if (commit()) reconcile();
    return;
  }
  if (!confirm) {
    if (view_.screen == Screen::CompleteConfirm) { show(Screen::Routine, int(index)); return; }
    if (view_.screen == Screen::Routine || view_.screen == Screen::AbandonConfirm) moveChoice(intent);
    return;
  }
  if (!time.trusted) { show(Screen::ClockRequired, int(index)); return; }
  if (view_.screen == Screen::Routine) {
    show(view_.selected ? Screen::CompleteConfirm : Screen::AbandonConfirm, int(index));
    return;
  }
  if (view_.screen == Screen::AbandonConfirm && !view_.selected) { show(Screen::Routine, int(index)); return; }
  domain::Action action;
  if (view_.screen == Screen::Letter) action = domain::Action::OpenLetter;
  else if (view_.screen == Screen::CompleteConfirm) action = domain::Action::CompleteStep;
  else if (view_.screen == Screen::AbandonConfirm) action = domain::Action::AbandonRun;
  else return;
  const auto& run = state_.routines[index].run;
  const auto change = domain::dispatch(run, {action, run.spec.id, view_.expectedVersion, view_.expectedStep}, time);
  if (transition(index, change)) reconcile();
}
}
