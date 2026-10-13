#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\domain\\RoutineEngine.cpp"
#include "RoutineEngine.h"
#include <limits>

namespace routine::domain {
namespace {
SpecError validateSpec(const RunSpec& spec, Policy policy) {
  if (!spec.id) return SpecError::MissingRunId;
  if (spec.startsAt < 0 || spec.endsAt <= spec.startsAt) return SpecError::InvalidWindow;
  if (!spec.stepCount || spec.stepCount > kMaxSteps) return SpecError::InvalidStepCount;
  if (policy.deadline != DeadlineRule::DeadlineWins && policy.deadline != DeadlineRule::AllowAtDeadline)
    return SpecError::InvalidPolicy;
  for (size_t i = 0; i < spec.stepCount; ++i) {
    if (!spec.steps[i]) return SpecError::MissingStepId;
    for (size_t j = 0; j < i; ++j)
      if (spec.steps[i] == spec.steps[j]) return SpecError::DuplicateStepId;
  }
  return SpecError::None;
}
Transition unchanged(const RunState& state, Result result = Result::NoChange) {
  Transition t;
  t.next = state;
  t.result = result;
  return t;
}
Result checkTime(const RunState& state, TimeSample now) {
  if (!now.trusted) return Result::ClockUnknown;
  if (now.unixSeconds < 0) return Result::InvalidTime;
  if (state.version && now.unixSeconds < state.lastTransitionAt) return Result::ClockMovedBackward;
  return Result::NoChange;
}
void emit(Transition& t, EventKind kind, int64_t when, StepId step = 0) {
  // Maximum transaction: warning + step complete + entitlement + run complete.
  t.events[t.eventCount++] = {t.next.spec.id, kind, step, when};
}
bool expired(const RunState& state, int64_t now) {
  return state.policy.deadline == DeadlineRule::DeadlineWins ? now >= state.spec.endsAt : now > state.spec.endsAt;
}
void advance(Transition& t, int64_t now) {
  auto& s = t.next;
  if (terminal(s.phase)) return;
  if (expired(s, now)) {
    s.phase = Phase::Expired;
    emit(t, EventKind::RunExpired, now);
    return;
  }
  if (s.phase == Phase::Scheduled && now >= s.spec.startsAt) {
    s.phase = Phase::LetterPending;
    emit(t, EventKind::Started, now);
  }
  const int64_t window = s.spec.endsAt - s.spec.startsAt;
  const int64_t lead = s.policy.warningLeadSeconds < window ? s.policy.warningLeadSeconds : window;
  if (s.phase != Phase::Scheduled && lead && !s.warningEmitted && now >= s.spec.endsAt - lead) {
    s.warningEmitted = true;
    emit(t, EventKind::DeadlineWarning, now);
  }
}
Transition finish(Transition t, const RunState& before, int64_t now) {
  if (t.changed()) {
    if (before.version == std::numeric_limits<uint32_t>::max()) return unchanged(before, Result::VersionExhausted);
    t.next.version = before.version + 1;
    t.next.lastTransitionAt = now;
    if (t.result == Result::NoChange) t.result = Result::Applied;
  }
  return t;
}
}

SpecError createRun(const RunSpec& spec, Policy policy, RunState& output) {
  const auto error = validateSpec(spec, policy);
  if (error != SpecError::None) return error;
  RunState created;
  created.spec = spec;
  created.policy = policy;
  output = created;
  return SpecError::None;
}
bool terminal(Phase phase) {
  return phase == Phase::Completed || phase == Phase::Abandoned || phase == Phase::Expired;
}
uint8_t countSteps(uint16_t mask) {
  uint8_t count=0; for (;mask;mask=uint16_t(mask&(mask-1))) ++count; return count;
}
uint8_t nextStep(const RunState& state) {
  for(uint8_t i=0;i<state.spec.stepCount;++i) if(!(completedMask(state)&(1u<<i))) return i;
  return state.spec.stepCount;
}
bool valid(const RunState& state) {
  if (validateSpec(state.spec, state.policy) != SpecError::None) return false;
  if (static_cast<unsigned>(state.phase) > static_cast<unsigned>(Phase::Expired)) return false;
  if (state.completedSteps > state.spec.stepCount) return false;
  const uint16_t mask = static_cast<uint16_t>((uint32_t(1) << state.spec.stepCount) - 1);
  if ((completedMask(state)&~mask) || countSteps(completedMask(state))!=state.completedSteps) return false;
  if ((state.phase == Phase::Completed) != (state.completedSteps == state.spec.stepCount)) return false;
  if ((state.phase == Phase::Scheduled || state.phase == Phase::LetterPending) && state.earnedRewards) return false;
  if (state.phase == Phase::Scheduled && (state.version || state.warningEmitted || state.lastTransitionAt)) return false;
  if (state.phase != Phase::Scheduled && (!state.version || state.lastTransitionAt < state.spec.startsAt)) return false;
  return true;
}
Transition advanceTime(const RunState& state, TimeSample now) {
  if (!valid(state)) return unchanged(state, Result::InvalidState);
  const auto clock = checkTime(state, now);
  if (clock != Result::NoChange) return unchanged(state, clock);
  auto t = unchanged(state);
  advance(t, now.unixSeconds);
  return finish(t, state, now.unixSeconds);
}
Transition dispatch(const RunState& state, Command command, TimeSample now) {
  if (!valid(state)) return unchanged(state, Result::InvalidState);
  if (command.run != state.spec.id) return unchanged(state, Result::WrongRun);
  if (command.expectedVersion != state.version) return unchanged(state, Result::StaleVersion);
  const auto clock = checkTime(state, now);
  if (clock != Result::NoChange) return unchanged(state, clock);
  auto t = unchanged(state);
  advance(t, now.unixSeconds);
  auto& s = t.next;
  if (s.phase == Phase::Expired) {
    t.result = Result::DeadlinePassed;
    return finish(t, state, now.unixSeconds);
  }
  switch (command.action) {
    case Action::OpenLetter:
      if (s.phase != Phase::LetterPending) { t.result = Result::WrongPhase; break; }
      s.phase = Phase::Active;
      emit(t, EventKind::LetterOpened, now.unixSeconds);
      break;
    case Action::CompleteStep:
      if (s.phase != Phase::Active) { t.result = Result::WrongPhase; break; }
      if (command.step != s.spec.steps[nextStep(s)]) { t.result = Result::WrongStep; break; }
      s.earnedRewards |= static_cast<uint16_t>(uint32_t(1) << nextStep(s));
      ++s.completedSteps;
      emit(t, EventKind::StepCompleted, now.unixSeconds, command.step);
      emit(t, EventKind::RewardEarned, now.unixSeconds, command.step);
      if (s.completedSteps == s.spec.stepCount) {
        s.phase = Phase::Completed;
        emit(t, EventKind::RunCompleted, now.unixSeconds);
      }
      break;
    case Action::AbandonRun:
      if (s.phase != Phase::Active && s.phase != Phase::LetterPending) { t.result = Result::WrongPhase; break; }
      s.phase = Phase::Abandoned;
      emit(t, EventKind::RunAbandoned, now.unixSeconds);
      break;
    default: t.result = Result::WrongPhase; break;
  }
  return finish(t, state, now.unixSeconds);
}
}
