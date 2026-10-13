#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\HardwareCheckRuntime.h"
#pragma once
#include "Board.h"
#include "NetworkClock.h"
#include "AssetStorage.h"
#include "../../diagnostics/HardwareCheck.h"
#include "../../diagnostics/LineBuffer.h"

namespace routine::platform {
// Product code will reuse Board/renderers, not add rules to the hardware check.
class HardwareCheckRuntime {
 public:
  void begin();
  void tick();
 private:
  void apply(diagnostics::Effects effects);
  void help();
  void command(const char* line);
  void status();
  void sleep(uint32_t seconds);
  void updateClockView();
  Board board_;
  NetworkClock clock_;
  AssetStorage assets_;
  diagnostics::HardwareCheck check_;
  diagnostics::BasicLineBuffer<224> lines_;
  ui::HomeViewModel displayView_{};
  uint32_t activityAt_ = 0, idleMs_ = 0, pendingSleep_ = 0;
  SleepResult lastSleep_{};
  application::SyncState lastSync_ = application::SyncState::Suspended;
  bool clockOverride_ = false;
  bool ready_ = false, dirty_ = true;
};
}
