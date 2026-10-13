#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\NetworkClock.h"
#pragma once
#include <cstdint>
#include "../../application/ClockSync.h"
#include "../../application/WifiProvisioning.h"
#include <esp_wifi_types.h>

namespace routine::platform {
struct ClockSettings {
  const char* timezone = "KST-9";  // Display setting, never used as an API date key.
  const char* primaryServer = "time.cloudflare.com";
  const char* secondaryServer = "pool.ntp.org";
};
// One instance owns the ESP32 system clock/SNTP service. All methods run on loop().
class NetworkClock : public provisioning::WifiPort {
 public:
  void begin(const ClockSettings& settings = {});
  void requestSync();  // Boot, every wake, and explicit retry all enter here.
  void tick();
  bool suspend();
  bool configure(const char* ssid, const char* password);
  provisioning::JoinState setupState()const{return join_.state();}
  void cancelSetup(){join_.cancel(*this);}
  bool tryCandidate(const char* ssid,const char* password) override;
  bool hasCandidateIp()const override;
  bool persistCandidate() override;
  void restorePrevious() override;
  bool configured() const { return configured_; }
  bool connected() const;
  bool localMinutes(uint16_t& minutes) const;
  int64_t unixSeconds() const;
  const application::ClockSync& sync() const { return sync_; }
  uint32_t requestCount() const { return requests_; }
  uint32_t successCount() const { return successes_; }
 private:
  void apply(application::SyncActions actions);
  void stopTime();
  bool loadSavedNetwork();
  bool applyNetwork(const wifi_config_t& config);
  wifi_config_t saved_{},candidate_{};
  provisioning::Join join_;
  ClockSettings settings_{};
  application::ClockSync sync_;
  bool configured_ = false, timeRunning_ = false;
  uint32_t requests_ = 0, successes_ = 0, receiptBaseline_ = 0;
};
}
