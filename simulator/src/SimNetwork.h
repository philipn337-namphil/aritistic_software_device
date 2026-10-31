#pragma once
#include "Console.h"
#include "../../firmware/RoutineDevice/src/application/ProductController.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

namespace routine::sim {
// Port of platform/esp32/DeviceNetwork: the same claim/sync state machine and
// the same portable api:: codec, with WinHTTP instead of the ESP32 HTTPClient
// and a settings file instead of NVS. Wi-Fi/BLE provisioning do not exist here.
class SimNetwork {
 public:
  enum class Status { Unconfigured, Unpaired, WaitingNetwork, WaitingClock, BatteryUnknown,
    Ready, Working, Retry, Unauthorized, InvalidResponse, MergeConflict, StorageError, ClaimUncertain };
  struct Clock { bool connected=true, known=true; int64_t unixSeconds=0; };
  SimNetwork(Console& console,std::string settingsFile):console_(console),file_(std::move(settingsFile)){}
  ~SimNetwork(){if(thread_.joinable())thread_.join();}
  void begin(uint32_t now);
  void tick(application::ProductController& product,const Clock& clock,int battery,bool allowed,uint32_t now);
  bool command(const char* line,uint32_t now);
  void request(){pending_=true;nextAt_=0;}
  bool busy() const { return working_.load()||identityPending_; }
  Status status() const { return status_; }
  const char* statusName() const;
  void printStatus() const;
  bool paired() const { return settings_.uuid[0]!=0; }
  const char* endpoint() const { return settings_.endpoint.c_str(); }
  int httpStatus() const { return httpStatus_.load(); }
  const std::string& deviceUid() const { return settings_.deviceUid; }
  const char* uuid() const { return settings_.uuid; }
 private:
  struct Settings {
    std::string endpoint;
    char uuid[37]{};
    uint64_t deviceId=0;
    uint32_t warningSeconds=0;
    uint8_t deadlineWins=1;
    bool claimUncertain=false;
    std::string deviceUid;
  } settings_{};
  bool saveSettings(const Settings& next);
  bool start(bool claim,const application::ProductState& state,int64_t now,int battery,uint32_t millis);
  void runRequest();
  void finish(application::ProductController& product,int64_t now,uint32_t millis);
  Console& console_;
  std::string file_;
  std::thread thread_;
  api::SyncRequest request_{};
  api::SyncResponse response_{};
  api::ClaimResponse claim_{};
  std::string requestBody_,responseBody_,url_,headers_;
  char pairingCode_[11]{};
  std::atomic<bool> working_{false},done_{false};
  bool claimJob_=false,pending_=true,storeReady_=false,identityPending_=false,halted_=false;
  Status status_=Status::Unconfigured;
  std::atomic<int> httpStatus_{0};
  uint32_t failures_=0,nextAt_=0,lastAttempt_=0;
  uint64_t generation_=0;
};
}
