#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\RewardLedger.cpp"
#include "RewardLedger.h"
#include "../content/Catalog.h"

namespace routine::application {
bool assignEarnedFood(RoutineRecord& record, uint32_t& randomState) {
  bool changed = false;
  for (size_t step = 0; step < record.run.spec.stepCount; ++step) {
    if(!(record.run.earnedRewards&(1u<<step))) continue;
    if (record.food[step]) continue;
    record.food[step] = content::drawFood(randomState);
    changed = true;
  }
  return changed;
}
int pendingRewardStep(const RoutineRecord& record, uint8_t food) {
  for (uint8_t step = 0; step < record.run.spec.stepCount; ++step) {
    if(!(record.run.earnedRewards&(1u<<step))) continue;
    if (record.consumedRewards & (1u << step)) continue;
    if (!food || record.food[step] == food) return step;
  }
  return -1;
}
RewardPosition findPendingFood(const ProductState& state, uint8_t food) {
  for (size_t index = 0; index < state.count; ++index) {
    const int step = pendingRewardStep(state.routines[index], food);
    if (step >= 0) return {static_cast<int8_t>(index), static_cast<uint8_t>(step)};
  }
  return {};
}
}
