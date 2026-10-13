#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\RewardLedger.h"
#pragma once
#include "ProductState.h"

namespace routine::application {
// Operates on a validated candidate only. The controller owns the atomic save.
bool assignEarnedFood(RoutineRecord& record, uint32_t& randomState);
// Returns the oldest unconsumed step, optionally restricted to a food ID; -1 if none.
int pendingRewardStep(const RoutineRecord& record, uint8_t food = 0);
struct RewardPosition {
  int8_t routine = -1;
  uint8_t step = 0;
  bool found() const { return routine >= 0; }
};
RewardPosition findPendingFood(const ProductState& state, uint8_t food);
}
