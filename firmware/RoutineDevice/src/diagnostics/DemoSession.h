#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\diagnostics\\DemoSession.h"
#pragma once
#include "../application/StateCodec.h"

namespace routine::diagnostics {
enum class DemoAction { Other, Start, Stop, Advance, Reload, Status, Invalid };
struct DemoCommand { DemoAction action=DemoAction::Other; uint32_t seconds=0; };
DemoCommand parseDemoCommand(const char* line);
bool demoAllowsCommand(const char* line);

// ProductController still owns all transitions. Only its storage and time
// dependencies change; this session has no filesystem, NVS or network access.
class DemoSession final : public application::ProductStore {
 public:
  explicit DemoSession(application::ProductStore& live):live_(live){}
  void start(uint32_t now){active_=true;size_=0;started_=now;advanced_=0;}
  void stop(){active_=false;size_=0;}
  bool active() const {return active_;}
  domain::TimeSample time(uint32_t now) const;
  bool advance(uint32_t seconds,uint32_t now);
  void schedule(application::Schedule& output) const;
  application::LoadResult load(application::ProductState& output) override;
  bool save(const application::ProductState& state) override;
  bool archive(const application::ProductState& state) override {return !active_&&live_.archive(state);}
  static constexpr int64_t kEpoch=1767225600; // Jan 1, 2026, 09:00 KST; simulation only.
 private:
  application::ProductStore& live_;
  uint8_t bytes_[application::kSnapshotBytes]{};
  size_t size_=0;
  uint32_t started_=0,advanced_=0;
  bool active_=false;
};
}
