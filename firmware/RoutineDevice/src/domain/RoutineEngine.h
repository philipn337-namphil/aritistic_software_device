#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\domain\\RoutineEngine.h"
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace routine::domain {
// Local opaque handles, NOT a definition of the server's ID/JSON format.
// The schedule adapter must preserve their meaning over a run's lifetime.
using RunId = uint64_t;
using StepId = uint32_t;
constexpr size_t kMaxSteps = 16;  // Implementation capacity, not product policy.

struct RunSpec {
  RunId id = 0;
  int64_t startsAt = 0;
  int64_t endsAt = 0;
  uint8_t stepCount = 0;
  std::array<StepId, kMaxSteps> steps{};
};
enum class DeadlineRule : uint8_t { DeadlineWins, AllowAtDeadline };
struct Policy {
  DeadlineRule deadline;
  uint32_t warningLeadSeconds;  // Zero disables warning; no hidden default.
};
enum class Phase : uint8_t { Scheduled, LetterPending, Active, Completed, Abandoned, Expired };
struct RunState {
  RunSpec spec{};
  Policy policy{DeadlineRule::DeadlineWins, 0};
  Phase phase = Phase::Scheduled;
  uint32_t version = 0;
  uint8_t completedSteps = 0;
  uint16_t earnedRewards = 0;  // One entitlement bit per completed step.
  uint16_t remoteCompleted = 0; // Server DONE never creates a second local reward.
  bool warningEmitted = false;
  int64_t lastTransitionAt = 0;
};
struct TimeSample {
  int64_t unixSeconds;
  bool trusted;
};
enum class Action : uint8_t { OpenLetter, CompleteStep, AbandonRun };
struct Command {
  Action action;
  RunId run;
  uint32_t expectedVersion;
  StepId step = 0;  // Required only for CompleteStep.
};
enum class EventKind : uint8_t {
  Started, LetterOpened, DeadlineWarning, StepCompleted,
  RewardEarned, RunCompleted, RunAbandoned, RunExpired
};
struct Event {
  RunId run;
  EventKind kind;
  StepId step;
  int64_t occurredAt;
};
// (run, kind, step) identifies each logical event within this engine. The API
// adapter will map this to the existing server's idempotency contract.
enum class Result : uint8_t {
  NoChange, Applied, InvalidState, ClockUnknown, InvalidTime,
  ClockMovedBackward, WrongRun, StaleVersion, WrongPhase, WrongStep,
  DeadlinePassed, VersionExhausted
};
enum class SpecError : uint8_t { None, MissingRunId, InvalidWindow, InvalidStepCount, MissingStepId, DuplicateStepId, InvalidPolicy };
struct Transition {
  RunState next{};
  std::array<Event, 4> events{};
  uint8_t eventCount = 0;
  Result result = Result::NoChange;
  bool changed() const { return eventCount != 0; }
};

SpecError createRun(const RunSpec& spec, Policy policy, RunState& output);
bool valid(const RunState& state);
bool terminal(Phase phase);
// Include the last second only when that run permits completion at its deadline.
inline bool executionWindowsOverlap(const RunState& a,const RunState& b){
  return (a.spec.startsAt<b.spec.endsAt&&b.spec.startsAt<a.spec.endsAt)||
    (a.spec.startsAt==b.spec.endsAt&&b.policy.deadline==DeadlineRule::AllowAtDeadline)||
    (b.spec.startsAt==a.spec.endsAt&&a.policy.deadline==DeadlineRule::AllowAtDeadline);
}
uint8_t nextStep(const RunState& state);
uint8_t countSteps(uint16_t mask);
inline uint16_t completedMask(const RunState& state) { return state.earnedRewards | state.remoteCompleted; }
Transition advanceTime(const RunState& state, TimeSample now);
Transition dispatch(const RunState& state, Command command, TimeSample now);
// These functions calculate only. The caller must atomically save next+events
// before publishing them. Even a rejected command can carry a deadline change;
// always inspect changed(), not just result == Applied. No I/O or heap allocation.
}
