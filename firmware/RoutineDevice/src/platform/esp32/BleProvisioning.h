#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\BleProvisioning.h"
#pragma once
#include "ProvisioningBuild.h"
#include "NetworkClock.h"
#include "DeviceNetwork.h"
#include <atomic>
#if ROUTINE_BLE_DEVELOPMENT
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
class BLEServer;
class BLECharacteristic;
#endif

namespace routine::platform {
struct BleCallbacks;
class BleProvisioning {
 public:
  bool start();
  void stop(NetworkClock& clock);
  void tick(NetworkClock& clock,DeviceNetwork& network);
  bool active()const{return active_;}
  const char* name()const{return name_;}
  const char* status()const{return status_;}
 private:
#if ROUTINE_BLE_DEVELOPMENT
  // NimBLE host-task callbacks only queue bytes; parsing runs in tick().
  friend struct BleCallbacks;
  struct Event {uint8_t kind=0,size=0;uint8_t data[20]{};};
  void advertise();
  void report(const char* status);
  void post(const Event& event);
  QueueHandle_t queue_=nullptr;
  std::atomic<bool> overflow_{false},linked_{false},failed_{false};
  std::atomic<uint16_t> connection_{0xffff};
  BLEServer* server_=nullptr;
  BLECharacteristic* statusCharacteristic_=nullptr;
  provisioning::Frame frame_;
  provisioning::Config pending_;
  bool joining_=false;
  uint32_t beganAt_=0,finishAt_=0;
#endif
  std::atomic<bool> active_{false};
  char name_[16]{};
  const char* status_="OFF";
};
}
