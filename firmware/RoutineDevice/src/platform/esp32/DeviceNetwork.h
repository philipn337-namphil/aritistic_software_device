#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\DeviceNetwork.h"
#pragma once
#include "NetworkClock.h"
#include "../../application/ProductController.h"
#include "../../application/ScheduleTransfer.h"
#include <atomic>
#include <string>

namespace routine::platform {
class DeviceNetwork {
 public:
  enum class Status { Unconfigured, Unpaired, WaitingNetwork, WaitingClock, BatteryUnknown,
    Ready, Working, Retry, Unauthorized, InvalidResponse, MergeConflict, StorageError, ClaimUncertain };
  void begin();
  void tick(application::ProductController& product,const NetworkClock& clock,int battery,bool allowed);
  bool command(const char* line);
  void request() { pending_=true; nextAt_=0; }
  bool busy() const { return working_.load() || identityPending_ || configTransfer_.active(); }
  Status status() const { return status_; }
  void printStatus() const;
  bool acceptProvisioning(const provisioning::Config& config);
  bool finishProvisioning(const provisioning::Config& config);
 private:
  struct Settings {
    uint32_t magic=0x41504931;
    char endpoint[192]{};
    char ca[4096]{};
    char uuid[37]{};
    uint64_t deviceId=0;
    uint32_t warningSeconds=0;
    uint8_t deadlineWins=1;
    bool claimUncertain=false;
  } settings_{};
  bool saveSettings(const Settings& next);
  bool queuePairing(const char* code);
  bool configure(const uint8_t* data,size_t size);
  bool start(bool claim,const application::ProductState& state,int64_t now,int battery);
  static void worker(void* context);
  void runRequest();
  void finish(application::ProductController& product,int64_t now);
  application::ScheduleTransfer configTransfer_;
  api::SyncRequest request_{};
  api::SyncResponse response_{};
  api::ClaimResponse claim_{};
  std::string requestBody_,responseBody_;
  char pairingCode_[11]{};
  std::atomic<bool> working_{false},done_{false};
  bool claimJob_=false,pending_=true,storeReady_=false,identityPending_=false,halted_=false;
  Status status_=Status::Unconfigured;
  std::atomic<int> httpStatus_{0};
  uint32_t failures_=0,nextAt_=0,lastAttempt_=0;
  uint64_t generation_=0;
};
}
