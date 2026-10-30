#pragma once
#include "Console.h"
#include "FileSlots.h"
#include "SimNetwork.h"
#include "../../firmware/RoutineDevice/src/application/ProductController.h"
#include "../../firmware/RoutineDevice/src/application/InputSession.h"
#include "../../firmware/RoutineDevice/src/application/ScheduleTransfer.h"
#include "../../firmware/RoutineDevice/src/diagnostics/DemoSession.h"
#include "../../firmware/RoutineDevice/src/input/ContextualGestures.h"
#include "../../firmware/RoutineDevice/src/presentation/Motion.h"
#include <deque>
#include <string>
#include <vector>

namespace routine::sim {
// Port of platform/esp32/ProductRuntime. The product controller, renderers,
// motion player, input session, gestures and demo session are the firmware's
// own classes; only the board (LCD, buttons, buzzer, light sleep), flash and
// clock are simulated here.
class SimRuntime {
 public:
  SimRuntime(const std::string& dataDirectory,const std::string& assetPack);
  bool begin();
  void tick();
  // Bit i = input::Control(i): 0 Previous (D4), 1 Confirm (D5), 2 Next (D6).
  void buttons(uint8_t mask);
  void command(const char* line);
  const uint16_t* pixels() const { return pixels_.data(); }
  uint32_t frames() const { return frames_; }
  uint32_t beeps() const { return beeps_; }
  bool screenOn() const { return !sleeping_; }
  Console& console() { return console_; }
  std::string statusJson() const;
  static uint32_t millis();
 private:
  domain::TimeSample time() const;
  int64_t liveSeconds() const;
  void status();
  bool demoCommand(const char* line);
  void resetPresentation();
  void dispatchInput(input::Intent intent);
  void armButton();
  void pumpGestures(uint32_t now);
  uint8_t level(uint32_t at);
  static constexpr uint32_t kMinHoldMs=60;
  bool inputBusy() const { return gestures_.busy()||buttons_||!events_.empty(); }
  void beep(uint16_t ms){buzzer_.start(millis(),ms);++beeps_;}
  bool canPresent() const { return !buzzer_.active(); }
  void present(){++frames_;}
  void sleep(uint32_t seconds);
  void loadSettings();
  Console console_;
  std::string directory_,assetPack_;
  std::vector<uint8_t> assetBytes_;
  std::vector<uint16_t> pixels_;
  FileSlots slots_;
  application::SnapshotStore store_;
  diagnostics::DemoSession demo_;
  application::ProductController product_;
  application::ScheduleTransfer transfer_;
  application::ProductState importScratch_{};
  application::Schedule schedule_{};
  SimNetwork network_;
  ui::MotionPlayer motion_;
  application::InputSession inputSession_;
  input::ContextualGestures gestures_;
  input::BuzzerPulse buzzer_;
  std::deque<input::InputEvent> events_;
  uint32_t inputContext_=0;
  uint8_t buttons_=0,requested_=0,held_=0;
  uint32_t pressedAt_[3]{};
  bool assetsReady_=false,ready_=false,dirty_=true,wifi_=true,sleeping_=false;
  uint32_t wakeAt_=0,sleepCount_=0;
  ui::MotionKind shownMotion_=ui::MotionKind::None;
  uint8_t shownFrame_=0;
  uint32_t revision_=0,activityAt_=0,idleMs_=60000,pendingSleep_=0,discardedInputs_=0,frames_=0,beeps_=0,lastGesture_=0;
  uint16_t minutes_=0;
  bool clockShown_=false,wifiShown_=false,liveMotionEnabled_=true;
  int64_t clockOffset_=0;
};
}
