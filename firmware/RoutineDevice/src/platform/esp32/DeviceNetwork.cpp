#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\DeviceNetwork.cpp"
#include "DeviceNetwork.h"
#include "../../application/DailySummary.h"
#include "../../application/JsonInput.h"
#include "../../Version.h"
#include "../../vendor/ArduinoJson.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <NetworkClient.h>
#include "ProvisioningBuild.h"
#include <Preferences.h>
#include <esp_system.h>
#include <cstring>
#include <algorithm>
#include <new>
#include <mbedtls/x509_crt.h>

namespace routine::platform {
namespace {
bool allowsDevelopmentHttp(const char* url){
  return kBleDevelopment&&url&&!std::strcmp(url,kDevelopmentServerUrl);
}

bool validEndpoint(const char* s){
  return provisioning::validServerUrl(s,false);
}
class BoundedBody : public Stream {
 public:
  explicit BoundedBody(std::string& body):body_(body){body_.clear();}
  size_t write(uint8_t byte) override{return write(&byte,1);}
  size_t write(const uint8_t* bytes,size_t size) override{
    if(size>api::kMaxJsonBytes-body_.size()){overflow=true;return 0;}
    body_.append(reinterpret_cast<const char*>(bytes),size);return size;
  }
  int available() override{return 0;}int read() override{return -1;}int peek() override{return -1;}void flush() override{}
  bool overflow=false;
 private:std::string& body_;
};
}
void DeviceNetwork::begin(){
  Preferences storage;storeReady_=storage.begin("routine-api",false);
  if(!storeReady_){status_=Status::StorageError;return;}
  if(storage.isKey("settings")){
    if(storage.getBytesLength("settings")!=sizeof(settings_)||storage.getBytes("settings",&settings_,sizeof(settings_))!=sizeof(settings_)||settings_.magic!=0x41504931){status_=Status::StorageError;storeReady_=false;return;}
    if(!std::memchr(settings_.endpoint,0,sizeof(settings_.endpoint))||!std::memchr(settings_.ca,0,sizeof(settings_.ca))||!std::memchr(settings_.uuid,0,sizeof(settings_.uuid))||(!provisioning::validServerUrl(settings_.endpoint,true)&&!allowsDevelopmentHttp(settings_.endpoint))||(std::strncmp(settings_.endpoint,"https://",8)==0&&!settings_.ca[0])||(settings_.uuid[0]&&!api::validUuid(settings_.uuid))){status_=Status::StorageError;storeReady_=false;return;}
  }
  // The development device is bound to the integration server while
  // retaining its existing device identity and pairing.
  if(kBleDevelopment&&std::strcmp(settings_.endpoint,kDevelopmentServerUrl)){
    auto* next=new(std::nothrow) Settings(settings_);
    if(!next){status_=Status::StorageError;storeReady_=false;return;}
    std::strcpy(next->endpoint,kDevelopmentServerUrl);next->ca[0]=0;
    const bool saved=saveSettings(*next);delete next;
    if(!saved){status_=Status::StorageError;storeReady_=false;return;}
  }
  status_=settings_.claimUncertain?Status::ClaimUncertain:!settings_.endpoint[0]?Status::Unconfigured:settings_.uuid[0]?Status::Ready:Status::Unpaired;
}
bool DeviceNetwork::acceptProvisioning(const provisioning::Config& config){
  if(!kBleDevelopment||!storeReady_||busy()||settings_.claimUncertain||!api::validPairingCode(config.pairingCode)||(!provisioning::validServerUrl(config.serverUrl,true)&&!allowsDevelopmentHttp(config.serverUrl)))return false;
  // Existing identities never move to an app-supplied origin or get re-claimed.
  if(settings_.uuid[0])return std::strcmp(settings_.endpoint,config.serverUrl)==0;
  if(!std::strncmp(config.serverUrl,"https://",8)&&(!settings_.ca[0]||std::strcmp(settings_.endpoint,config.serverUrl)))return false;
  // Cancel an older unsent code, but retain the server/CA until Wi-Fi commits.
  provisioning::erase(pairingCode_,sizeof(pairingCode_));return true;
}
bool DeviceNetwork::finishProvisioning(const provisioning::Config& config){
  if(!acceptProvisioning(config))return false;
  if(settings_.uuid[0])return true;
  if(!std::strcmp(settings_.endpoint,config.serverUrl))return queuePairing(config.pairingCode);
  auto* next=new(std::nothrow) Settings(settings_);if(!next)return false;
  std::strcpy(next->endpoint,config.serverUrl);next->ca[0]=0;
  const bool ok=saveSettings(*next);delete next;
  return ok&&queuePairing(config.pairingCode);
}
bool DeviceNetwork::queuePairing(const char* code){
  if(!storeReady_||busy()||settings_.claimUncertain||!settings_.endpoint[0])return false;
  if(settings_.uuid[0])return true; // Wi-Fi replacement retains account identity.
  if(!api::validPairingCode(code))return false;
  std::strcpy(pairingCode_,code);pending_=true;halted_=false;nextAt_=0;return true;
}
bool DeviceNetwork::saveSettings(const Settings& next){
  if(!storeReady_)return false;
  Preferences storage;if(!storage.begin("routine-api",false))return false;
  if(storage.putBytes("settings",&next,sizeof(next))!=sizeof(next))return false;
  // Compare in small chunks through getBytes's bounded destination (settings
  // itself is replaced only after a complete round-trip verification).
  auto* check=new(std::nothrow) Settings;
  if(!check)return false;
  const bool ok=storage.getBytesLength("settings")==sizeof(next)&&storage.getBytes("settings",check,sizeof(next))==sizeof(next)&&!std::memcmp(check,&next,sizeof(next));
  delete check;if(ok)settings_=next;return ok;
}
bool DeviceNetwork::configure(const uint8_t* data,size_t size){
  if(!data||!size||size>8192||working_.load()||identityPending_)return false;
  JsonDocument doc;if(!api::readJson(doc,reinterpret_cast<const char*>(data),size,3)||!doc.is<JsonObject>())return false;
  const char* endpoint=api::jsonText(doc["baseUrl"]);const char* ca=api::jsonText(doc["rootCaPem"]);
  if(!validEndpoint(endpoint)||!ca||std::strlen(ca)>=sizeof(settings_.ca)||!std::strstr(ca,"-----BEGIN CERTIFICATE-----"))return false;
  mbedtls_x509_crt certificate;mbedtls_x509_crt_init(&certificate);
  const int parsed=mbedtls_x509_crt_parse(&certificate,reinterpret_cast<const unsigned char*>(ca),std::strlen(ca)+1);mbedtls_x509_crt_free(&certificate);if(parsed)return false;
  // Never silently forward an existing credential to another server.
  auto* next=new(std::nothrow) Settings(settings_);if(!next)return false;
  std::strcpy(next->endpoint,endpoint);size_t n=std::strlen(endpoint);if(n&&next->endpoint[n-1]=='/')next->endpoint[n-1]=0;
  if((settings_.uuid[0]||settings_.claimUncertain)&&std::strcmp(settings_.endpoint,next->endpoint)){delete next;return false;}
  std::strcpy(next->ca,ca);
  if(!doc["warningLeadSeconds"].isNull()){
    if(!doc["warningLeadSeconds"].is<uint32_t>()||doc["warningLeadSeconds"].as<uint32_t>()>86400){delete next;return false;}next->warningSeconds=doc["warningLeadSeconds"];
  }
  if(!doc["deadlineRule"].isNull()){
    const char* rule=api::jsonText(doc["deadlineRule"]);if(!rule||(std::strcmp(rule,"deadline-wins")&&std::strcmp(rule,"allow-at-deadline"))){delete next;return false;}next->deadlineWins=!std::strcmp(rule,"deadline-wins");
  }
  const bool ok=saveSettings(*next);delete next;if(ok){halted_=false;pending_=true;nextAt_=0;}return ok;
}
bool DeviceNetwork::command(const char* line){
  if(std::strncmp(line,"api ",4))return false;
  if(!std::strcmp(line,"api status")){printStatus();return true;}
  if(!std::strcmp(line,"api reset-uuid")){
    if(working_.load()||identityPending_||configTransfer_.active()){Serial.println("API ERR busy");return true;}
    auto* next=new(std::nothrow) Settings(settings_);
    if(!next){Serial.println("API ERR storage");return true;}
    next->uuid[0]=0;next->deviceId=0;next->claimUncertain=false;
    const bool ok=saveSettings(*next);delete next;
    if(ok){std::memset(pairingCode_,0,sizeof(pairingCode_));halted_=false;pending_=false;nextAt_=0;failures_=0;status_=settings_.endpoint[0]?Status::Unpaired:Status::Unconfigured;Serial.println("API UUID RESET");}
    else Serial.println("API ERR storage");
    return true;
  }
  if(working_.load()||identityPending_){Serial.println("API ERR busy");return true;}
  if(!std::strncmp(line,"api config ",11)){
    char command[224];std::snprintf(command,sizeof(command),"load %s",line+11);
    const auto result=configTransfer_.receive(command,millis());
    if(result==application::TransferResult::Complete){Serial.println(configure(configTransfer_.bytes(),configTransfer_.size())?"API CONFIG OK":"API ERR config");}
    else if(result==application::TransferResult::Invalid)Serial.println("API ERR transfer");
    else Serial.printf("API CONFIG %u\n",unsigned(configTransfer_.received()));
    return true;
  }
  if(!std::strcmp(line,"api sync")){pending_=true;halted_=false;nextAt_=0;Serial.println("API sync requested");return true;}
  if(!std::strncmp(line,"api pair ",9)){
    if(!api::validPairingCode(line+9)||settings_.uuid[0]||settings_.claimUncertain||!settings_.endpoint[0])Serial.println("API ERR pairing state");
    else{std::strcpy(pairingCode_,line+9);pending_=true;nextAt_=0;halted_=false;Serial.println("API pairing requested");}
    return true;
  }
  Serial.println("API ERR command");return true;
}
void DeviceNetwork::printStatus()const{
  static const char* const names[]={"unconfigured","unpaired","waiting-network","waiting-clock","battery-unknown","ready","working","retry","unauthorized","invalid-response","merge-conflict","storage-error","claim-uncertain"};
  Serial.printf("API state=%s configured=%d paired=%d busy=%d http=%d failures=%lu\n",names[unsigned(status_)],settings_.endpoint[0]!=0,settings_.uuid[0]!=0,working_.load(),httpStatus_.load(),static_cast<unsigned long>(failures_));
}
bool DeviceNetwork::start(bool claim,const application::ProductState& state,int64_t now,int battery){
  claimJob_=claim;requestBody_.clear();responseBody_.clear();httpStatus_=0;
  if(claim){
    char uid[32];std::snprintf(uid,sizeof(uid),"YESO-%012llX",static_cast<unsigned long long>(ESP.getEfuseMac()));
    if(!api::makeClaim(pairingCode_,uid,kFirmwareVersion,requestBody_))return false;
    auto* next=new(std::nothrow) Settings(settings_);if(!next)return false;next->claimUncertain=true;
    const bool saved=saveSettings(*next);delete next;if(!saved){status_=Status::StorageError;halted_=true;return false;}
  }else if(!api::prepareSync(state,now,battery,request_)||!api::makeSync(request_,kFirmwareVersion,requestBody_))return false;
  done_=false;working_=true;status_=Status::Working;pending_=false;generation_=state.generation;lastAttempt_=millis();
  if(xTaskCreate(worker,"device-api",12288,this,1,nullptr)!=pdPASS){
    working_=false;
    if(claim){
      // No worker exists, so no request was sent. Clear the durable intent
      // before retrying the still-unused code; failure remains conservative.
      auto* next=new(std::nothrow) Settings(settings_);bool cleared=false;
      if(next){next->claimUncertain=false;cleared=saveSettings(*next);delete next;}
      if(!cleared){status_=Status::ClaimUncertain;halted_=true;std::memset(pairingCode_,0,sizeof(pairingCode_));}
    }
    std::fill(requestBody_.begin(),requestBody_.end(),'\0');requestBody_.clear();return false;
  }
  if(claim)std::memset(pairingCode_,0,sizeof(pairingCode_));
  return true;
}
void DeviceNetwork::worker(void* context){auto* self=static_cast<DeviceNetwork*>(context);self->runRequest();self->done_.store(true,std::memory_order_release);vTaskDelete(nullptr);}
void DeviceNetwork::runRequest(){
  NetworkClientSecure socket;socket.setCACert(settings_.ca);socket.setHandshakeTimeout(10);
  NetworkClient localSocket;
  HTTPClient http;http.setConnectTimeout(8000);http.setTimeout(8000);http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  const std::string url=std::string(settings_.endpoint)+(claimJob_?"/device-api/v1/claim":"/device-api/v1/sync");
  const bool tls=!std::strncmp(settings_.endpoint,"https://",8);
  if(!tls&&(!kBleDevelopment||!allowsDevelopmentHttp(settings_.endpoint))){httpStatus_=-4;return;}
  if(!http.begin(tls?static_cast<NetworkClient&>(socket):localSocket,url.c_str())){httpStatus_=-1;return;}
  http.addHeader("Content-Type","application/json");
  if(!claimJob_)http.addHeader("X-Device-Uuid",settings_.uuid);
  httpStatus_=http.POST(reinterpret_cast<uint8_t*>(&requestBody_[0]),requestBody_.size());
  if(httpStatus_==200){BoundedBody body(responseBody_);const int length=http.getSize();
    if(length>int(api::kMaxJsonBytes))httpStatus_=-2;
    else {const int received=http.writeToStream(&body);if(body.overflow)httpStatus_=-2;else if(received<0)httpStatus_=-3;}
  }
  http.end();std::fill(requestBody_.begin(),requestBody_.end(),'\0');requestBody_.clear();
}
void DeviceNetwork::finish(application::ProductController& product,int64_t now){
  working_=false;done_=false;
  if(claimJob_){
    if(httpStatus_==200&&api::parseClaim(responseBody_.data(),responseBody_.size(),claim_)==api::Result::Ok){identityPending_=true;nextAt_=0;}
    else {
      // A positive rejection is known not to have claimed. Ambiguous transport,
      // 5xx and malformed success require recovery, never automatic code reuse.
      if(httpStatus_==400||httpStatus_==404||httpStatus_==409||httpStatus_==410){auto* next=new(std::nothrow) Settings(settings_);if(next){next->claimUncertain=false;if(!saveSettings(*next))status_=Status::StorageError;delete next;}}
      status_=settings_.claimUncertain?Status::ClaimUncertain:Status::Unpaired;halted_=true;
    }
  }else if(httpStatus_==200){
    const auto policy=domain::Policy{settings_.deadlineWins?domain::DeadlineRule::DeadlineWins:domain::DeadlineRule::AllowAtDeadline,settings_.warningSeconds};
    const auto parsed=api::parseSync(responseBody_.data(),responseBody_.size(),response_,policy);
    auto result=parsed;
    if(parsed==api::Result::Ok){
      product.retireAcknowledgedDays(application::koreanDay(now));
      result=product.applySync(response_,request_);
      if(result==api::Result::Ok)product.retireAcknowledgedDays(application::koreanDay(now));
    }
    status_=result==api::Result::Ok?Status::Ready:result==api::Result::StorageError?Status::StorageError:result==api::Result::Conflict?Status::MergeConflict:Status::InvalidResponse;
    if(result==api::Result::Ok){failures_=0;nextAt_=millis()+300000;generation_=product.state().generation;}
    else{halted_=true;}
  }else if(httpStatus_==-2){status_=Status::InvalidResponse;halted_=true;}
  else if(httpStatus_==401){status_=Status::Unauthorized;halted_=true;}
  else if(httpStatus_>0&&httpStatus_<500&&httpStatus_!=429){status_=Status::InvalidResponse;halted_=true;}
  else{++failures_;status_=Status::Retry;pending_=true;nextAt_=millis()+std::min<uint32_t>(300000,10000u<<(std::min<uint32_t>(failures_,5)-1));}
  std::fill(responseBody_.begin(),responseBody_.end(),'\0');responseBody_.clear();printStatus();
}
void DeviceNetwork::tick(application::ProductController& product,const NetworkClock& clock,int battery,bool allowed){
  configTransfer_.tick(millis());
  if(done_.load(std::memory_order_acquire))finish(product,clock.unixSeconds());
  if(identityPending_){
    if(nextAt_&&int32_t(millis()-nextAt_)<0)return;
    auto* next=new(std::nothrow) Settings(settings_);if(!next)return;
    next->deviceId=claim_.deviceId;std::strcpy(next->uuid,claim_.uuid.data());next->claimUncertain=false;
    if(saveSettings(*next)){identityPending_=false;pending_=true;halted_=false;status_=Status::Ready;claim_={};}else status_=Status::StorageError;
    delete next;nextAt_=millis()+5000;return;
  }
  if(!allowed||working_.load()||configTransfer_.active()||halted_||product.faulted())return;
  if(!storeReady_){status_=Status::StorageError;return;}
  if(!settings_.endpoint[0]){status_=Status::Unconfigured;return;}
  if(!provisioning::validServerUrl(settings_.endpoint,kBleDevelopment)&&!allowsDevelopmentHttp(settings_.endpoint)){status_=Status::Unconfigured;return;}
  if(settings_.claimUncertain){status_=Status::ClaimUncertain;return;}
  const bool claim=pairingCode_[0]!=0;
  if(!claim&&!settings_.uuid[0]){status_=Status::Unpaired;return;}
  if(!clock.connected()){status_=Status::WaitingNetwork;return;}
  if(!clock.sync().known()){status_=Status::WaitingClock;return;}
  if(!claim&&battery<0){status_=Status::BatteryUnknown;return;}
  const uint32_t now=millis();
  if(nextAt_&&int32_t(now-nextAt_)<0){
    if(!(product.state().generation!=generation_&&uint32_t(now-lastAttempt_)>=2000&&failures_==0))return;
  }
  if(pending_||claim||product.state().generation!=generation_||!nextAt_||int32_t(now-nextAt_)>=0){
    if(!start(claim,product.state(),clock.unixSeconds(),battery)&&!halted_){status_=Status::Retry;pending_=true;lastAttempt_=now;generation_=product.state().generation;nextAt_=now+30000;}
  }
}
}
