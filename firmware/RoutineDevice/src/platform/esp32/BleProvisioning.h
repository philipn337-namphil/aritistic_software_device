#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\BleProvisioning.h"
#pragma once
#include "ProvisioningBuild.h"
#include "NetworkClock.h"
#include "DeviceNetwork.h"
#include <atomic>
#if ROUTINE_BLE_DEVELOPMENT
#include <esp_gatts_api.h>
#include <esp_gap_ble_api.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace routine::platform {
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
  struct Event {uint8_t kind=0,size=0;uint8_t data[20]{};};
  static void gap(esp_gap_ble_cb_event_t event,esp_ble_gap_cb_param_t* param);
  static void gatt(esp_gatts_cb_event_t event,esp_gatt_if_t interface,esp_ble_gatts_cb_param_t* param);
  void advertise();
  void report(const char* status);
  void post(const Event& event);
  QueueHandle_t queue_=nullptr;
  std::atomic<bool> overflow_{false},linked_{false},failed_{false};
  std::atomic<uint8_t> advertisementReady_{0};
  std::atomic<uint16_t> connection_{0xffff};
  uint16_t handles_[6]{};
  esp_gatt_if_t interface_=ESP_GATT_IF_NONE;
  std::atomic<bool> subscribed_{false};
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
