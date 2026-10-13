#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\ProductRuntime.h"
#pragma once
#include "Board.h"
#include "NetworkClock.h"
#include "FlashSlots.h"
#include "AssetStorage.h"
#include "DeviceNetwork.h"
#include "BleProvisioning.h"
#include "../../application/ProductController.h"
#include "../../application/InputSession.h"
#include "../../application/ScheduleTransfer.h"
#include "../../diagnostics/LineBuffer.h"
#include "../../diagnostics/DemoSession.h"
#include "../../presentation/Motion.h"

namespace routine::platform {
class ProductRuntime {
 public:
  ProductRuntime() : store_(flash_), demo_(store_), product_(demo_) {}
  void begin();
  void tick();
 private:
  domain::TimeSample time() const;
  void command(const char* line);
  bool demoCommand(const char* line);
  void resetPresentation();
  void dispatchInput(input::Intent intent);
  void status();
  void sleep(uint32_t seconds);
  Board board_;
  NetworkClock clock_;
  DeviceNetwork network_;
  BleProvisioning ble_;
  bool setupRequested_=false;
  const char* shownSetupStatus_=nullptr;
  FlashSlots flash_;
  AssetStorage assets_;
  application::SnapshotStore store_;
  diagnostics::DemoSession demo_;
  application::ProductController product_;
  application::ScheduleTransfer transfer_;
  application::ProductState importScratch_{};
  application::Schedule schedule_{};
  diagnostics::BasicLineBuffer<224> lines_;
  ui::MotionPlayer motion_;
  application::InputSession inputSession_;
  ui::MotionKind shownMotion_=ui::MotionKind::None;
  uint8_t shownFrame_=0;
  bool ready_=false,dirty_=true;
  uint32_t revision_=0,activityAt_=0,idleMs_=60000,pendingSleep_=0;
  uint32_t discardedInputs_=0;
  uint16_t minutes_=0;
  bool clockShown_=false;
  bool wifiShown_=false;
  bool liveMotionEnabled_=true;
  SleepResult lastSleep_{};
};
}
