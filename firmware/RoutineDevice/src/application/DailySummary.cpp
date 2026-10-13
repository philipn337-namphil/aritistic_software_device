#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\DailySummary.cpp"
#include "DailySummary.h"

namespace routine::application {
int64_t koreanDay(int64_t unixSeconds) {
  if (unixSeconds < 0) return kUnknownDay;
  return unixSeconds/86400 + (unixSeconds%86400 + 9*3600)/86400;
}
DailySummary summarizeDay(const ProductState& state, int64_t day) {
  DailySummary summary;
  if (day < 0 || state.count > kMaxRuns) return summary;
  summary.day = day;
  for (size_t i = 0; i < state.count; ++i) {
    const auto& run = state.routines[i].run;
    if (koreanDay(run.spec.startsAt) != day) continue;
    ++summary.runs;
    summary.steps += run.spec.stepCount;
    summary.completedSteps += run.completedSteps;
    if (run.phase == domain::Phase::Completed) ++summary.completedRuns;
    else if (run.phase == domain::Phase::Abandoned) ++summary.abandonedRuns;
    else if (run.phase == domain::Phase::Expired) ++summary.expiredRuns;
  }
  return summary;
}
SummaryPage summaryPage(const ProductState& state, int64_t day, uint8_t page) {
  SummaryPage result;
  if (day < 0 || state.count > kMaxRuns) return result;
  uint8_t order[kMaxRuns]{};
  size_t count = 0;
  for (uint8_t i = 0; i < state.count; ++i) {
    const auto& spec = state.routines[i].run.spec;
    if (koreanDay(spec.startsAt) != day || !spec.stepCount || spec.stepCount > domain::kMaxSteps) continue;
    size_t at = count++;
    while (at) {
      const auto& prior = state.routines[order[at-1]].run.spec;
      if (prior.startsAt < spec.startsAt || (prior.startsAt == spec.startsAt && prior.id <= spec.id)) break;
      order[at] = order[at-1]; --at;
    }
    order[at] = i;
  }
  unsigned remaining = page;
  for (size_t i = 0; i < count; ++i) {
    const auto steps = state.routines[order[i]].run.spec.stepCount;
    const auto pages = uint8_t((steps+kSummaryRows-1)/kSummaryRows);
    if (!result.valid() && remaining < pages) {
      result.routine = static_cast<int8_t>(order[i]);
      result.firstStep = uint8_t(remaining*kSummaryRows);
      result.stepCount = uint8_t(steps-result.firstStep < kSummaryRows ? steps-result.firstStep : kSummaryRows);
    }
    if (!result.valid()) remaining -= pages;
    result.pages += pages;
  }
  return result;
}
}
