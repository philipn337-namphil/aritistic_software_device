#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\ProductState.cpp"
#include "ProductState.h"
#include "DailySummary.h"
#include "../content/Catalog.h"
#include <cstring>

namespace routine::application {
DailyRoutineProgress dailyRoutineProgress(const ProductState& state, size_t index) {
  if (state.count > kMaxRuns || index >= state.count) return {};
  // Run timestamps are nonnegative. Split first to avoid overflowing int64_t.
  const auto& selected = state.routines[index].run.spec;
  DailyRoutineProgress progress{1,0};
  for (size_t i = 0; i < state.count; ++i) {
    const auto& other = state.routines[i].run.spec;
    if (koreanDay(other.startsAt) != koreanDay(selected.startsAt)) continue;
    ++progress.total;
    if (other.startsAt < selected.startsAt || (other.startsAt == selected.startsAt && other.id < selected.id)) ++progress.ordinal;
  }
  return progress;
}
FoodInventory foodInventory(const ProductState& state, uint8_t food) {
  FoodInventory inventory;
  if (!food || food>content::kFoodCount || state.count>kMaxRuns) return inventory;
  inventory.acquired=state.archivedFood[food-1];
  for (size_t i=0;i<state.count;++i) {
    const auto& record=state.routines[i];
    for (uint8_t s=0;s<record.run.spec.stepCount;++s) if ((record.run.earnedRewards&(1u<<s)) && record.food[s]==food) {
      if(inventory.acquired!=UINT32_MAX)++inventory.acquired;
      if (!(record.consumedRewards & (1u<<s))) ++inventory.available;
    }
  }
  return inventory;
}
bool validText(const char* text, size_t capacity) {
  if (!text || !*text) return false;
  size_t i = 0;
  while (i < capacity && text[i]) {
    const auto first = static_cast<uint8_t>(text[i++]);
    if (first < 0x80) { if (first < 32 || first == 127) return false; continue; }
    unsigned count; uint32_t cp, minimum;
    if (first >= 0xc2 && first <= 0xdf) { count = 1; cp = first & 31; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { count = 2; cp = first & 15; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { count = 3; cp = first & 7; minimum = 0x10000; }
    else return false;
    while (count--) {
      if (i >= capacity) return false;
      const auto next = static_cast<uint8_t>(text[i++]);
      if ((next & 0xc0) != 0x80) return false;
      cp = (cp << 6) | (next & 63);
    }
    if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
  }
  return i < capacity;
}
bool validProduct(const ProductState& state) {
  if (state.count > kMaxRuns || state.eventCount > kMaxEvents || (state.count && !state.scheduleRevision)) return false;
  if (!domain::validCompanion(state.companion,content::kCharacterCount) || !state.randomState ||
      state.summarySeenDay < -1 || state.summarySeenDay > INT64_MAX/86400+1 ||
      state.retiredBeforeDay < -1 || state.retiredBeforeDay > INT64_MAX/86400+1) return false;
  for (size_t i = 0; i < state.count; ++i) {
    const auto& record = state.routines[i];
    if (!domain::valid(record.run) || !validText(record.title.data()) ||
        (record.acknowledgedRewards & ~record.run.earnedRewards) ||
        (record.resultSeen && !domain::terminal(record.run.phase)) ||
        (record.consumedRewards & ~record.acknowledgedRewards) ||
        (record.syncedRewards & ~record.run.earnedRewards)) return false;
    for (size_t s=0;s<domain::kMaxSteps;++s) {
      if (record.food[s]>content::kFoodCount || (!(record.run.earnedRewards&(1u<<s)) && record.food[s])) return false;
      if ((record.consumedRewards & (1u<<s)) && !record.food[s]) return false;
    }
    for (size_t j = 0; j < record.run.spec.stepCount; ++j) if (!validText(record.stepNames[j].data())) return false;
    for (size_t j = 0; j < i; ++j) if (state.routines[j].run.spec.id == record.run.spec.id) return false;
  }
  for (size_t i = 0; i < state.eventCount; ++i) {
    const auto& event = state.events[i];
    if (!event.run || event.occurredAt < 0 || static_cast<unsigned>(event.kind) > static_cast<unsigned>(domain::EventKind::RunExpired)) return false;
    bool found = false;
    for (size_t r = 0; r < state.count; ++r) if (state.routines[r].run.spec.id == event.run) {
      found = true;
      const auto& run = state.routines[r].run;
      if (event.occurredAt < run.spec.startsAt || event.occurredAt > run.lastTransitionAt) return false;
      if (event.kind == domain::EventKind::StepCompleted || event.kind == domain::EventKind::RewardEarned) {
        bool stepFound = false;
        for (size_t s = 0; s < run.spec.stepCount; ++s) if ((run.earnedRewards&(1u<<s)) && run.spec.steps[s] == event.step) stepFound = true;
        if (!stepFound) return false;
      } else if (event.step) return false;
    }
    if (!found) return false;
    for (size_t j = 0; j < i; ++j) {
      const auto& previous = state.events[j];
      if (previous.run == event.run && previous.kind == event.kind && previous.step == event.step) return false;
    }
  }
  return true;
}
}
