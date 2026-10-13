#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\NetworkClock.cpp"
#include "NetworkClock.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_sntp.h>
#include <atomic>
#include <cstring>
#include <ctime>
#include <Preferences.h>
#include "../../application/StateCodec.h"

namespace routine::platform {
namespace {
std::atomic<uint32_t> timeReceipts{0};
constexpr int64_t kEarliestTime = 1577836800;  // Reject an unset clock (2020-01-01).
struct SavedWifi {
  uint32_t magic=0x57494631;
  char ssid[33]{},password[64]{};
  uint8_t reserved[3]{};
  uint32_t crc=0;
};
void receivedTime(struct timeval* value) {
  // Called on the TCP/IP task; never render or mutate product state here.
  if (value && value->tv_sec >= kEarliestTime) timeReceipts.fetch_add(1);
}
}
void NetworkClock::begin(const ClockSettings& settings) {
  settings_ = settings;
  WiFi.persistent(false); // Candidate settings must never overwrite the last good network.
  WiFi.setAutoReconnect(false);  // Bounded attempts are owned by ClockSync.
  configured_ = loadSavedNetwork();
  requestSync();
}
bool NetworkClock::loadSavedNetwork() {
  if (!WiFi.mode(WIFI_STA)) return false;
  Preferences storage;if(!storage.begin("routine-wifi",false))return false;
  if(storage.isKey("good")){
    SavedWifi data{};
    const bool valid=storage.getBytesLength("good")==sizeof(data)&&storage.getBytes("good",&data,sizeof(data))==sizeof(data)&&
      data.magic==0x57494631&&data.crc==application::crc32(reinterpret_cast<const uint8_t*>(&data),offsetof(SavedWifi,crc))&&
      std::memchr(data.ssid,0,sizeof(data.ssid))&&data.ssid[0]&&std::memchr(data.password,0,sizeof(data.password));
    if(valid){std::memcpy(saved_.sta.ssid,data.ssid,std::strlen(data.ssid));std::memcpy(saved_.sta.password,data.password,std::strlen(data.password));}
    provisioning::erase(&data,sizeof(data));return valid;
  }
  // Upgrade old USB-configured devices without erasing their SDK NVS settings.
  return esp_wifi_get_config(WIFI_IF_STA,&saved_)==ESP_OK&&saved_.sta.ssid[0];
}
bool NetworkClock::applyNetwork(const wifi_config_t& config){
  if(!WiFi.mode(WIFI_STA)||!WiFi.STA.begin(false)||!WiFi.STA.disconnect(false,1000))return false;
  auto copy=config;
  const bool ok=esp_wifi_set_storage(WIFI_STORAGE_RAM)==ESP_OK&&esp_wifi_set_config(WIFI_IF_STA,&copy)==ESP_OK;
  provisioning::erase(&copy,sizeof(copy));return ok&&WiFi.STA.connect();
}
bool NetworkClock::connected() const { return WiFi.status() == WL_CONNECTED; }
void NetworkClock::stopTime() {
  if (timeRunning_) esp_sntp_stop();  // ESP-IDF's thread-safe wrapper.
  timeRunning_ = false;
}
void NetworkClock::apply(application::SyncActions actions) {
  if (actions.stopTime) stopTime();
  if (actions.connect) {
    applyNetwork(saved_);
  }
  if (actions.requestTime) {
    stopTime();
    receiptBaseline_ = timeReceipts.load();
    esp_sntp_set_time_sync_notification_cb(receivedTime);
    configTzTime(settings_.timezone, settings_.primaryServer, settings_.secondaryServer);
    timeRunning_ = true;
  }
}
void NetworkClock::requestSync() {
  ++requests_;
  if(join_.state()==provisioning::JoinState::Connecting)return;
  apply(sync_.request(millis(), configured_, connected()));
}
void NetworkClock::tick() {
  if(join_.state()==provisioning::JoinState::Connecting){
    join_.tick(*this,millis());
    if(join_.state()!=provisioning::JoinState::Connecting)requestSync();
    return;
  }
  const bool received = timeRunning_ && timeReceipts.load() != receiptBaseline_;
  const bool wasFresh = sync_.fresh();
  apply(sync_.tick(millis(), connected(), received));
  if (!wasFresh && sync_.fresh()) ++successes_;
}
bool NetworkClock::suspend() {
  if(join_.state()==provisioning::JoinState::Connecting)return false;
  apply(sync_.suspend());
  return WiFi.mode(WIFI_OFF);  // Credentials survive; every wake reconnects.
}
bool NetworkClock::configure(const char* ssid, const char* password) {
  if (!ssid || !password || !*ssid || std::strlen(ssid) > 32) return false;
  const size_t length = std::strlen(password);
  if (length && (length < 8 || length > 63)) return false;
  for (size_t i = 0; i < length; ++i)
    if (static_cast<unsigned char>(password[i]) < 32 || static_cast<unsigned char>(password[i]) > 126) return false;
  provisioning::Config candidate;std::strcpy(candidate.ssid,ssid);std::strcpy(candidate.password,password);
  const bool ok=join_.start(*this,candidate,millis());candidate.clear();
  if(!ok&&join_.state()!=provisioning::JoinState::Connecting)requestSync();return ok;
}
bool NetworkClock::tryCandidate(const char* ssid,const char* password){
  apply(sync_.suspend());candidate_={};
  std::memcpy(candidate_.sta.ssid,ssid,std::strlen(ssid));std::memcpy(candidate_.sta.password,password,std::strlen(password));
  return applyNetwork(candidate_);
}
bool NetworkClock::hasCandidateIp()const{return connected()&&uint32_t(WiFi.localIP())!=0;}
bool NetworkClock::persistCandidate(){
  SavedWifi data;std::memcpy(data.ssid,candidate_.sta.ssid,32);std::memcpy(data.password,candidate_.sta.password,63);
  data.crc=application::crc32(reinterpret_cast<const uint8_t*>(&data),offsetof(SavedWifi,crc));
  Preferences storage;const bool ok=storage.begin("routine-wifi",false)&&storage.putBytes("good",&data,sizeof(data))==sizeof(data);
  provisioning::erase(&data,sizeof(data));
  if(ok){saved_=candidate_;configured_=true;provisioning::erase(&candidate_,sizeof(candidate_));}return ok;
}
void NetworkClock::restorePrevious(){
  provisioning::erase(&candidate_,sizeof(candidate_));
  if(configured_)applyNetwork(saved_);else WiFi.disconnect(false,false);
}
int64_t NetworkClock::unixSeconds() const { return static_cast<int64_t>(time(nullptr)); }
bool NetworkClock::localMinutes(uint16_t& minutes) const {
  if (!sync_.known()) return false;
  const time_t now = time(nullptr);
  struct tm local{};
  if (now < kEarliestTime || !localtime_r(&now, &local)) return false;
  minutes = static_cast<uint16_t>(local.tm_hour * 60 + local.tm_min);
  return true;
}
}
