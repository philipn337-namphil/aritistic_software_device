#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\Board.h"
#pragma once
#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include "../../input/Intent.h"
#include "../../input/ContextualGestures.h"
#include "../../input/SystemGestures.h"
#include "ProvisioningBuild.h"
#include <esp_sleep.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

namespace routine::platform {
struct SleepResult {
  esp_err_t error = ESP_OK;
  esp_sleep_wakeup_cause_t cause = ESP_SLEEP_WAKEUP_UNDEFINED;
};
class Board {
 public:
  Board();
  bool begin();
  uint16_t* pixels() { return pixels_; }
  void present();
  void armButton();
  bool takeSetupRequest(){return setupRequested_.exchange(false);}
  input::InputEvent pollInput();
  input::Intent pollIntent(){return pollInput().intent;} // Diagnostic mode.
  void inputContext(uint32_t context){inputContext_=context;}
  bool inputBusy() const { return gestureBusy_.load() || pressed() != 0 || (inputQueue_ && uxQueueMessagesWaiting(inputQueue_)); }
  SleepResult lightSleep(uint32_t seconds);
  void updateBuzzer();
  void beep(uint16_t durationMs);
  bool canPresent() const { return !buzzer_.active(); }
 private:
  uint8_t pressed() const;  // Bit mask over hardware::kButtons.
  void buzzerLevel(bool active);
  void disableButtonWakeup();
  static void inputTask(void* context);
  Adafruit_ST7789 display_;
  uint16_t* pixels_ = nullptr;
#if ROUTINE_BLE_DEVELOPMENT
  input::SystemGestures gestures_;
#else
  input::ContextualGestures gestures_;
#endif
  std::atomic<bool> setupRequested_{false};
  input::BuzzerPulse buzzer_;
  TaskHandle_t inputTask_=nullptr;
  QueueHandle_t inputQueue_=nullptr;
  std::atomic<bool> gestureBusy_{false},inputOverflow_{false};
  std::atomic<uint32_t> inputContext_{1};
};
}
