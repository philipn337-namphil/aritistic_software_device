#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\ProductController.cpp"
#include "ProductController.h"
#include "RewardLedger.h"
#include "../content/Catalog.h"
#include <cstring>
#include <limits>

namespace routine::application {
namespace {
bool sameDefinition(const RoutineRecord& a, const RoutineRecord& b) {
  const auto& x = a.run.spec; const auto& y = b.run.spec;
  if (x.id != y.id || x.startsAt != y.startsAt || x.endsAt != y.endsAt || x.stepCount != y.stepCount ||
      a.run.policy.deadline != b.run.policy.deadline || a.run.policy.warningLeadSeconds != b.run.policy.warningLeadSeconds ||
      std::strcmp(a.title.data(), b.title.data())) return false;
  for (size_t i = 0; i < x.stepCount; ++i)
    if (x.steps[i] != y.steps[i] || std::strcmp(a.stepNames[i].data(), b.stepNames[i].data())) return false;
  return true;
}
}
LoadResult ProductController::begin() {
  candidate_ = {};
  const auto result = store_.load(candidate_);
  fault_ = result != LoadResult::Loaded && result != LoadResult::Empty;
  if (result == LoadResult::Loaded || result == LoadResult::Recovered) state_ = candidate_;
  else state_ = {};
  view_ = {};
  now_ = {0,false};
  if (!fault_ && result==LoadResult::Loaded) migrateRewards();
  if (fault_) fail(); else reconcile();
  ++viewRevision_;
  return result;
}
void ProductController::fail() { fault_ = true; show(Screen::StorageError); }
void ProductController::show(Screen screen, int index) {
  view_.screen = screen;
  view_.routine = static_cast<int8_t>(index);
  view_.selected = (screen == Screen::AbandonConfirm || screen == Screen::Catalog ||
                    screen == Screen::CharacterConfirm || screen == Screen::NewCharacter) ? 0 : 1;
  if (screen==Screen::CharacterCatalog) view_.item=0;
  if (screen==Screen::RewardCatalog) view_.item=0;
  if (screen==Screen::SummaryIntro) view_.item=0;
  view_.expectedVersion=0; view_.expectedStep=0;
  view_.warning = index >= 0 && state_.routines[index].run.warningEmitted;
  if (index >= 0) {
    const auto& r = state_.routines[index].run;
    view_.expectedVersion = r.version;
    view_.expectedStep = domain::nextStep(r) < r.spec.stepCount ? r.spec.steps[domain::nextStep(r)] : 0;
  }
  ++viewRevision_;
}
bool ProductController::commit() {
  if (fault_ || state_.generation == std::numeric_limits<uint64_t>::max()) { fail(); return false; }
  candidate_.generation = state_.generation + 1;
  if (!validProduct(candidate_) || !store_.save(candidate_)) { fail(); return false; }
  state_ = candidate_;
  ++viewRevision_;
  return true;
}
ImportResult ProductController::importSchedule(const Schedule& schedule) {
  if (fault_) return ImportResult::StorageError;
  if (!schedule.revision || !schedule.count || schedule.count > kMaxRuns) return ImportResult::Invalid;
  candidate_ = {};
  candidate_.companion=state_.companion;
  candidate_.randomState=state_.randomState;
  candidate_.summarySeenDay=state_.summarySeenDay;
  candidate_.scheduleRevision = schedule.revision;
  candidate_.count = schedule.count;
  candidate_.routines = schedule.routines;
  if (!validProduct(candidate_)) return ImportResult::Invalid;
  for (size_t i = 0; i < schedule.count; ++i) {
    const auto& record = schedule.routines[i];
    if (record.run.phase != domain::Phase::Scheduled || record.acknowledgedRewards || record.resultSeen || record.consumedRewards) return ImportResult::Invalid;
    for (size_t j = 0; j < i; ++j) {
      if(domain::executionWindowsOverlap(record.run,schedule.routines[j].run))return ImportResult::Conflict;
    }
  }
  if (state_.scheduleRevision) {
    if (schedule.revision == state_.scheduleRevision && schedule.count == state_.count) {
      bool same = true;
      for (size_t i = 0; i < state_.count; ++i) same = same && sameDefinition(state_.routines[i], schedule.routines[i]);
      return same ? ImportResult::Duplicate : ImportResult::Conflict;
    }
    // USB import remains a first-setup tool. API updates use applySync instead.
    return ImportResult::Conflict;
  }
  if (!commit()) return ImportResult::StorageError;
  reconcile();
  return ImportResult::Accepted;
}
bool ProductController::transition(size_t index, const domain::Transition& change) {
  if (!change.changed()) return false;
  if (state_.eventCount + change.eventCount > kMaxEvents) { fail(); return false; }
  candidate_ = state_;
  candidate_.routines[index].run = change.next;
  assignEarnedFood(candidate_.routines[index], candidate_.randomState);
  for (size_t i = 0; i < change.eventCount; ++i) candidate_.events[candidate_.eventCount++] = change.events[i];
  if (!commit()) return false;
  for (size_t i = 0; i < change.eventCount; ++i)
    if (change.events[i].kind == domain::EventKind::Started || change.events[i].kind == domain::EventKind::DeadlineWarning ||
        change.events[i].kind == domain::EventKind::RunExpired) alert_ = true;
  return true;
}
int ProductController::currentRoutine() const {
  int active = -1;
  for (size_t i = 0; i < state_.count; ++i) {
    const auto& record = state_.routines[i];
    if ((record.run.earnedRewards & ~record.consumedRewards) ||
        (domain::terminal(record.run.phase) && !record.resultSeen)) return static_cast<int>(i);
    if (!domain::terminal(record.run.phase) && record.run.phase != domain::Phase::Scheduled) active = static_cast<int>(i);
  }
  return active;
}
void ProductController::reconcile() {
  if (fault_) return;
  if (state_.companion.notice!=domain::GrowthNotice::None) { show(Screen::Evolution); return; }
  if (state_.companion.offered!=domain::kNoCharacter) { show(Screen::NewCharacter); return; }
  const int index = currentRoutine();
  if (index < 0) {
    if (view_.summary.finished() && state_.summarySeenDay!=view_.summary.day) show(Screen::SummaryIntro);
    else show(Screen::Home);
    return;
  }
  const auto& record = state_.routines[index];
  if (record.run.earnedRewards & ~record.consumedRewards) presentReward(index);
  else if (domain::terminal(record.run.phase)) show(Screen::Result, index);
  else if (record.run.phase == domain::Phase::LetterPending) show(Screen::Letter, index);
  else show(Screen::Routine, index);
}
bool ProductController::migrateRewards() {
  candidate_ = state_;
  bool changed = false;
  for (size_t i = 0; i < candidate_.count; ++i)
    changed = assignEarnedFood(candidate_.routines[i], candidate_.randomState) || changed;
  return !changed || commit();
}
void ProductController::presentReward(int index, int step) {
  const auto& record = state_.routines[size_t(index)];
  if (step < 0) step = pendingRewardStep(record);
  if (step < 0) return;
  const bool acknowledged = record.acknowledgedRewards & (1u << step);
  show(acknowledged ? Screen::Feed : Screen::StepSaved, index);
  view_.rewardStep = uint8_t(step);
}
bool ProductController::feedReward() {
  if (view_.routine<0 || view_.routine>=state_.count) return false;
  const auto& record=state_.routines[size_t(view_.routine)];
  if (view_.rewardStep>=record.run.spec.stepCount || !(record.run.earnedRewards&(1u<<view_.rewardStep))) return false;
  const uint16_t bit=uint16_t(1u<<view_.rewardStep);
  if (!(record.acknowledgedRewards & bit) || record.consumedRewards & bit) return false;
  candidate_=state_;
  if (!domain::feedCompanion(candidate_.companion,content::kCharacterCount)) {
    show(Screen::CharacterCatalog); // Fully grown: choose or raise a character again.
    return false;
  }
  candidate_.routines[size_t(view_.routine)].consumedRewards |= bit;
  if (!commit()) return false;
  if (state_.companion.notice!=domain::GrowthNotice::None) reconcile();
  else show(Screen::Growth);
  return true;
}
void ProductController::updateSummary(domain::TimeSample time) {
  const auto summary=summarizeDay(state_,time.trusted?koreanDay(time.unixSeconds):kUnknownDay);
  const auto& old=view_.summary;
  if (summary.day!=old.day || summary.runs!=old.runs || summary.steps!=old.steps ||
      summary.completedSteps!=old.completedSteps || summary.completedRuns!=old.completedRuns ||
      summary.abandonedRuns!=old.abandonedRuns || summary.expiredRuns!=old.expiredRuns) {
    view_.summary=summary;
    ++viewRevision_;
  }
}
void ProductController::tick(domain::TimeSample time) {
  now_=time;
  const auto previousDay=view_.summary.day;
  if (view_.clockKnown != time.trusted) { view_.clockKnown = time.trusted; ++viewRevision_; }
  if (fault_) return;
  bool changed = false;
  for (size_t i = 0; i < state_.count && !fault_; ++i) {
    const auto change = domain::advanceTime(state_.routines[i].run, time);
    if (change.result == domain::Result::ClockMovedBackward) {
      now_.trusted=false;
      if (view_.screen != Screen::ClockRequired) show(Screen::ClockRequired, static_cast<int>(i));
      return;
    }
    changed = transition(i, change) || changed;
  }
  updateSummary(time);
  const bool inSummary = isSummaryScreen(view_.screen);
  if (inSummary && !time.trusted) { show(Screen::ClockRequired); return; }
  // Invalidate an open confirmation when its version changes (warning/expiry).
  if (changed || (time.trusted && view_.screen == Screen::ClockRequired) ||
      (previousDay!=view_.summary.day && (inSummary || view_.screen==Screen::Home))) reconcile();
}
uint32_t ProductController::sleepSeconds(domain::TimeSample now, uint32_t maximum) const {
  if (fault_ || !maximum) return 0;
  if (!now.trusted) return maximum < 30 ? maximum : 30;
  int64_t wait = maximum;
  for (size_t i = 0; i < state_.count; ++i) {
    const auto& run = state_.routines[i].run;
    if (domain::terminal(run.phase)) continue;
    const int64_t end = run.spec.endsAt + (run.policy.deadline == domain::DeadlineRule::AllowAtDeadline && run.spec.endsAt < INT64_MAX ? 1 : 0);
    const int64_t boundary = run.phase == domain::Phase::Scheduled ? run.spec.startsAt : end;
    if (boundary <= now.unixSeconds) return 0;
    if (boundary - now.unixSeconds < wait) wait = boundary - now.unixSeconds;
    if (run.policy.warningLeadSeconds && !run.warningEmitted) {
      const int64_t warning = run.spec.endsAt - run.policy.warningLeadSeconds;
      if (warning > now.unixSeconds && warning - now.unixSeconds < wait) wait = warning - now.unixSeconds;
    }
  }
  return static_cast<uint32_t>(wait);
}
}
