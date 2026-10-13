#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\StateCodec.cpp"
#include "StateCodec.h"
#include <cstring>

namespace routine::application {
namespace {
struct Writer {
  uint8_t* data; size_t capacity, position = 0; bool ok = true;
  void integer(uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i) { if (position < capacity) data[position++] = uint8_t(value >> (i * 8)); else ok = false; }
  }
  void text(const Text& value) {
    const size_t n = std::strlen(value.data()); integer(n, 1);
    for (size_t i = 0; i < n; ++i) integer(uint8_t(value[i]), 1);
  }
};
struct Reader {
  const uint8_t* data; size_t size, position = 0; bool ok = true;
  uint64_t integer(unsigned bytes) {
    uint64_t result = 0;
    for (unsigned i = 0; i < bytes; ++i) { if (position < size) result |= uint64_t(data[position++]) << (i * 8); else ok = false; }
    return result;
  }
  bool boolean() { const auto value = integer(1); if (value > 1) ok = false; return value == 1; }
  int64_t timestamp() { const auto value = integer(8); if (value > INT64_MAX) ok = false; return int64_t(value & INT64_MAX); }
  void text(Text& value) {
    const auto n = integer(1); if (!n || n >= value.size()) { ok = false; return; }
    for (size_t i = 0; i < n; ++i) { value[i] = char(integer(1)); if (!value[i]) ok = false; }
    value[n] = 0;
  }
};
bool rewardsAssigned(const ProductState& state) {
  for (size_t i=0;i<state.count;++i)
    for (size_t step=0;step<state.routines[i].run.spec.stepCount;++step)
      if ((state.routines[i].run.earnedRewards&(1u<<step)) && !state.routines[i].food[step]) return false;
  return true;
}
}
uint32_t crc32(const uint8_t* bytes, size_t size) {
  uint32_t crc = 0xffffffff;
  for (size_t i = 0; i < size; ++i) {
    crc ^= bytes[i];
    for (unsigned j = 0; j < 8; ++j) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
  }
  return ~crc;
}
size_t encodeState(const ProductState& state, uint8_t* bytes, size_t capacity) {
  if (!bytes || !validProduct(state) || !rewardsAssigned(state)) return 0;
  Writer w{bytes, capacity};
  w.integer(0x54534452, 4); w.integer(4, 2); w.integer(state.generation, 8); w.integer(state.scheduleRevision, 8); w.integer(state.count, 1);
  for (size_t i = 0; i < state.count; ++i) {
    const auto& record = state.routines[i]; const auto& r = record.run;
    w.integer(r.spec.id, 8); w.integer(r.spec.startsAt, 8); w.integer(r.spec.endsAt, 8); w.integer(r.spec.stepCount, 1);
    w.integer(unsigned(r.policy.deadline), 1); w.integer(r.policy.warningLeadSeconds, 4);
    w.integer(unsigned(r.phase), 1); w.integer(r.version, 4); w.integer(r.completedSteps, 1); w.integer(r.earnedRewards, 2);
    w.integer(r.warningEmitted, 1); w.integer(r.lastTransitionAt, 8); w.integer(record.acknowledgedRewards, 2); w.integer(record.resultSeen, 1);
    w.text(record.title);
    for (size_t s = 0; s < r.spec.stepCount; ++s) { w.integer(r.spec.steps[s], 4); w.text(record.stepNames[s]); }
    w.integer(record.consumedRewards,2);
    for (const auto food : record.food) w.integer(food,1);
  }
  w.integer(state.eventCount, 2);
  for (size_t i = 0; i < state.eventCount; ++i) {
    const auto& e = state.events[i]; w.integer(e.run, 8); w.integer(unsigned(e.kind), 1); w.integer(e.step, 4); w.integer(e.occurredAt, 8);
  }
  w.integer(state.companion.owned,8);
  for (auto experience : state.companion.experience) w.integer(experience,2);
  w.integer(state.companion.selected,1); w.integer(state.companion.offered,1); w.integer(unsigned(state.companion.notice),1);
  w.integer(state.randomState,4); w.integer(uint64_t(state.summarySeenDay),8);
  for (uint8_t i=0; i<domain::kMaxCharacters; ++i) w.integer(domain::collectedStage(state.companion,i),1);
  for(size_t i=0;i<state.count;++i) {
    w.integer(state.routines[i].run.remoteCompleted,2);
    w.integer(state.routines[i].syncedRewards,2);
    w.integer(state.routines[i].apiManaged,1);
  }
  for(auto count:state.archivedFood) w.integer(count,4);
  w.integer(uint64_t(state.retiredBeforeDay),8);
  if (!w.ok) return 0;
  w.integer(crc32(bytes, w.position), 4);
  return w.ok ? w.position : 0;
}
DecodeResult decodeState(const uint8_t* bytes, size_t size, ProductState& state) {
  if (!bytes || size < 29 || size > kSnapshotBytes) return DecodeResult::Invalid;
  Reader tail{bytes + size - 4, 4};
  if (tail.integer(4) != crc32(bytes, size - 4)) return DecodeResult::Invalid;
  Reader r{bytes, size - 4};
  if (r.integer(4) != 0x54534452) return DecodeResult::Invalid;
  const auto schema=r.integer(2);
  if (schema<1 || schema>4) return DecodeResult::Unsupported;
  state = {}; // Caller owns scratch memory; publish only after Ok.
  state.generation = r.integer(8); state.scheduleRevision = r.integer(8); state.count = uint8_t(r.integer(1));
  if (state.count > kMaxRuns) return DecodeResult::Invalid;
  for (size_t i = 0; i < state.count; ++i) {
    auto& record = state.routines[i]; auto& run = record.run;
    run.spec.id = r.integer(8); run.spec.startsAt = r.timestamp(); run.spec.endsAt = r.timestamp(); run.spec.stepCount = uint8_t(r.integer(1));
    if (run.spec.stepCount > domain::kMaxSteps) return DecodeResult::Invalid;
    run.policy.deadline = domain::DeadlineRule(r.integer(1)); run.policy.warningLeadSeconds = uint32_t(r.integer(4));
    run.phase = domain::Phase(r.integer(1)); run.version = uint32_t(r.integer(4)); run.completedSteps = uint8_t(r.integer(1));
    run.earnedRewards = uint16_t(r.integer(2)); run.warningEmitted = r.boolean(); run.lastTransitionAt = r.timestamp();
    record.acknowledgedRewards = uint16_t(r.integer(2)); record.resultSeen = r.boolean(); r.text(record.title);
    for (size_t s = 0; s < run.spec.stepCount; ++s) { run.spec.steps[s] = uint32_t(r.integer(4)); r.text(record.stepNames[s]); }
    if (schema>=2) {
      record.consumedRewards=uint16_t(r.integer(2));
      for (auto& food : record.food) food=uint8_t(r.integer(1));
    }
  }
  state.eventCount = uint16_t(r.integer(2)); if (state.eventCount > kMaxEvents) return DecodeResult::Invalid;
  for (size_t i = 0; i < state.eventCount; ++i) {
    auto& e = state.events[i]; e.run = r.integer(8); e.kind = domain::EventKind(r.integer(1)); e.step = uint32_t(r.integer(4)); e.occurredAt = r.timestamp();
  }
  if (schema>=2) {
    state.companion.owned=r.integer(8);
    for (auto& experience : state.companion.experience) experience=uint16_t(r.integer(2));
    state.companion.selected=uint8_t(r.integer(1)); state.companion.offered=uint8_t(r.integer(1));
    state.companion.notice=domain::GrowthNotice(r.integer(1));
    state.randomState=uint32_t(r.integer(4));
    const auto day=r.integer(8);
    if (day!=UINT64_MAX && day>uint64_t(INT64_MAX/86400+1)) return DecodeResult::Invalid;
    state.summarySeenDay=day==UINT64_MAX ? -1 : int64_t(day);
  }
  if (schema>=3) {
    for (auto& stage : state.companion.highestStage) stage=uint8_t(r.integer(1));
  } else {
    for (uint8_t i=0; i<domain::kMaxCharacters; ++i)
      state.companion.highestStage[i]=domain::collectedStage(state.companion,i);
  }
  if(schema>=4) {
    for(size_t i=0;i<state.count;++i) {
      state.routines[i].run.remoteCompleted=uint16_t(r.integer(2));
      state.routines[i].syncedRewards=uint16_t(r.integer(2));
      state.routines[i].apiManaged=r.boolean();
    }
    for(auto& count:state.archivedFood) count=uint32_t(r.integer(4));
    const auto day=r.integer(8);
    if(day!=UINT64_MAX && day>uint64_t(INT64_MAX/86400+1)) return DecodeResult::Invalid;
    state.retiredBeforeDay=day==UINT64_MAX?-1:int64_t(day);
  }
  return r.ok && r.position == r.size && validProduct(state) && (schema==1 || rewardsAssigned(state))
      ? DecodeResult::Ok : DecodeResult::Invalid;
}
}
