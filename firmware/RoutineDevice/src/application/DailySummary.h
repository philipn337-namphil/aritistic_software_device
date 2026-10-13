#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\DailySummary.h"
#pragma once
#include "ProductState.h"

namespace routine::application {
constexpr int64_t kUnknownDay = -1;
int64_t koreanDay(int64_t unixSeconds);

struct DailySummary {
  int64_t day = kUnknownDay;
  uint16_t steps = 0;
  uint16_t completedSteps = 0;
  uint8_t runs = 0;
  uint8_t completedRuns = 0;
  uint8_t abandonedRuns = 0;
  uint8_t expiredRuns = 0;
  bool finished() const {
    return runs && runs == completedRuns + abandonedRuns + expiredRuns;
  }
};

// Count by the routine's scheduled KST date, preserving already completed steps
// when the remainder is abandoned or expires. Tomorrow is never today's denominator.
DailySummary summarizeDay(const ProductState& state, int64_t day);
constexpr uint8_t kSummaryRows = 3;
struct SummaryPage {
  int8_t routine = -1;
  uint8_t firstStep = 0, stepCount = 0, pages = 0;
  bool valid() const { return routine >= 0; }
};
// Figma's letter pages, ordered by scheduled time then stable ID. A long
// routine continues on another sheet; no title/outcome is copied into UI state.
SummaryPage summaryPage(const ProductState& state, int64_t day, uint8_t page);
}
