#include "SimNetwork.h"
#include "FileSlots.h"
#include "HttpClient.h"
#include "../../firmware/RoutineDevice/src/application/DailySummary.h"
#include "../../firmware/RoutineDevice/src/platform/esp32/ProvisioningBuild.h"
#include "../../firmware/RoutineDevice/src/Version.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <random>

namespace routine::sim {
void SimNetwork::begin(uint32_t){
  Settings loaded{};std::string value;
  if(readSetting(file_,"endpoint",value))loaded.endpoint=value;
  if(readSetting(file_,"uuid",value)&&value.size()<sizeof(loaded.uuid))std::strcpy(loaded.uuid,value.c_str());
  if(readSetting(file_,"deviceId",value))loaded.deviceId=std::strtoull(value.c_str(),nullptr,10);
  if(readSetting(file_,"warningSeconds",value))loaded.warningSeconds=uint32_t(std::strtoul(value.c_str(),nullptr,10));
  if(readSetting(file_,"deadlineWins",value))loaded.deadlineWins=value!="0";
  if(readSetting(file_,"claimUncertain",value))loaded.claimUncertain=value=="1";
  if(readSetting(file_,"deviceUid",value))loaded.deviceUid=value;
  if(loaded.uuid[0]&&!api::validUuid(loaded.uuid)){status_=Status::StorageError;return;}
  // Like the development firmware, the virtual device is bound to the
  // integration server. The device UID replaces the ESP32 eFuse MAC.
  bool changed=false;
  if(loaded.endpoint.empty()){loaded.endpoint=platform::kDevelopmentServerUrl;changed=true;}
  if(loaded.deviceUid.empty()){
    std::random_device random;const uint64_t mac=((uint64_t(random())<<32)|random())&0xffffffffffffull;
    char uid[32];std::snprintf(uid,sizeof(uid),"YESO-%012llX",static_cast<unsigned long long>(mac));
    loaded.deviceUid=uid;changed=true;
  }
  storeReady_=true;
  if(changed&&!saveSettings(loaded)){storeReady_=false;status_=Status::StorageError;return;}
  settings_=loaded;
  status_=settings_.claimUncertain?Status::ClaimUncertain:settings_.endpoint.empty()?Status::Unconfigured:settings_.uuid[0]?Status::Ready:Status::Unpaired;
}
bool SimNetwork::saveSettings(const Settings& next){
  if(!storeReady_)return false;
  const bool ok=writeSetting(file_,"endpoint",next.endpoint)&&writeSetting(file_,"uuid",next.uuid)
    &&writeSetting(file_,"deviceId",std::to_string(next.deviceId))&&writeSetting(file_,"warningSeconds",std::to_string(next.warningSeconds))
    &&writeSetting(file_,"deadlineWins",next.deadlineWins?"1":"0")&&writeSetting(file_,"claimUncertain",next.claimUncertain?"1":"0")
    &&writeSetting(file_,"deviceUid",next.deviceUid);
  if(ok)settings_=next;return ok;
}
const char* SimNetwork::statusName() const {
  static const char* const names[]={"unconfigured","unpaired","waiting-network","waiting-clock","battery-unknown","ready","working","retry","unauthorized","invalid-response","merge-conflict","storage-error","claim-uncertain"};
  return names[unsigned(status_)];
}
void SimNetwork::printStatus() const {
  console_.printf("API state=%s configured=%d paired=%d busy=%d http=%d failures=%lu\n",statusName(),!settings_.endpoint.empty(),settings_.uuid[0]!=0,working_,httpStatus_,static_cast<unsigned long>(failures_));
}
bool SimNetwork::command(const char* line,uint32_t){
  if(std::strncmp(line,"api ",4))return false;
  if(!std::strcmp(line,"api status")){printStatus();console_.printf("API endpoint=%s uid=%s uuid=%s\n",settings_.endpoint.c_str(),settings_.deviceUid.c_str(),settings_.uuid[0]?settings_.uuid:"-");return true;}
  if(!std::strcmp(line,"api reset-uuid")){
    if(working_||identityPending_){console_.println("API ERR busy");return true;}
    Settings next=settings_;next.uuid[0]=0;next.deviceId=0;next.claimUncertain=false;
    if(saveSettings(next)){std::memset(pairingCode_,0,sizeof(pairingCode_));halted_=false;pending_=false;nextAt_=0;failures_=0;status_=settings_.endpoint.empty()?Status::Unconfigured:Status::Unpaired;console_.println("API UUID RESET");}
    else console_.println("API ERR storage");
    return true;
  }
  if(working_||identityPending_){console_.println("API ERR busy");return true;}
  if(!std::strcmp(line,"api sync")){pending_=true;halted_=false;nextAt_=0;console_.println("API sync requested");return true;}
  if(!std::strncmp(line,"api pair ",9)){
    if(!api::validPairingCode(line+9)||settings_.uuid[0]||settings_.claimUncertain||settings_.endpoint.empty())console_.println("API ERR pairing state");
    else{std::strcpy(pairingCode_,line+9);pending_=true;nextAt_=0;halted_=false;console_.println("API pairing requested");}
    return true;
  }
  if(!std::strncmp(line,"api endpoint ",13)){
    // Simulator-only: the device never moves an existing identity to another origin.
    const char* url=line+13;
    if(settings_.uuid[0]||settings_.claimUncertain||(std::strncmp(url,"http://",7)&&std::strncmp(url,"https://",8))){console_.println("API ERR endpoint (reset-uuid first)");return true;}
    Settings next=settings_;next.endpoint=url;if(!next.endpoint.empty()&&next.endpoint.back()=='/')next.endpoint.pop_back();
    console_.println(saveSettings(next)?"API ENDPOINT OK":"API ERR storage");return true;
  }
  console_.println("API ERR command");return true;
}
bool SimNetwork::start(bool claim,const application::ProductState& state,int64_t now,int battery,uint32_t millis){
  claimJob_=claim;requestBody_.clear();responseBody_.clear();httpStatus_=0;
  if(claim){
    if(!api::makeClaim(pairingCode_,settings_.deviceUid.c_str(),kFirmwareVersion,requestBody_))return false;
    Settings next=settings_;next.claimUncertain=true;
    if(!saveSettings(next)){status_=Status::StorageError;halted_=true;return false;}
  }else if(!api::prepareSync(state,now,battery,request_)||!api::makeSync(request_,kFirmwareVersion,requestBody_))return false;
  working_=true;status_=Status::Working;pending_=false;generation_=state.generation;lastAttempt_=millis;
  url_=settings_.endpoint+(claim?"/device-api/v1/claim":"/device-api/v1/sync");
  headers_=claim?std::string():std::string("X-Device-Uuid: ")+settings_.uuid+"\r\n";
  console_.printf("API POST %s\n",url_.c_str());
  http_.start(url_,headers_,requestBody_,api::kMaxJsonBytes);
  std::fill(requestBody_.begin(),requestBody_.end(),'\0');requestBody_.clear();
  if(claim)std::memset(pairingCode_,0,sizeof(pairingCode_));
  return true;
}
void SimNetwork::finish(application::ProductController& product,int64_t now,uint32_t millis){
  working_=false;httpStatus_=http_.status();responseBody_=http_.take();
  console_.printf("API HTTP %d (%u bytes)\n",httpStatus_,unsigned(responseBody_.size()));
  if(claimJob_){
    if(httpStatus_==200&&api::parseClaim(responseBody_.data(),responseBody_.size(),claim_)==api::Result::Ok){identityPending_=true;nextAt_=0;}
    else {
      // A positive rejection is known not to have claimed. Ambiguous transport,
      // 5xx and malformed success require recovery, never automatic code reuse.
      if(httpStatus_==400||httpStatus_==404||httpStatus_==409||httpStatus_==410){Settings next=settings_;next.claimUncertain=false;if(!saveSettings(next))status_=Status::StorageError;}
      status_=settings_.claimUncertain?Status::ClaimUncertain:Status::Unpaired;halted_=true;
      if(!responseBody_.empty())console_.printf("API body %.300s\n",responseBody_.c_str());
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
    console_.printf("API sync parse=%u apply=%u routines=%u\n",unsigned(parsed),unsigned(result),unsigned(response_.count));
    status_=result==api::Result::Ok?Status::Ready:result==api::Result::StorageError?Status::StorageError:result==api::Result::Conflict?Status::MergeConflict:Status::InvalidResponse;
    if(result==api::Result::Ok){failures_=0;nextAt_=millis+300000;generation_=product.state().generation;}
    else{halted_=true;console_.printf("API body %.300s\n",responseBody_.c_str());}
  }else if(httpStatus_==-2){status_=Status::InvalidResponse;halted_=true;}
  else if(httpStatus_==401){status_=Status::Unauthorized;halted_=true;}
  else if(httpStatus_>0&&httpStatus_<500&&httpStatus_!=429){status_=Status::InvalidResponse;halted_=true;console_.printf("API body %.300s\n",responseBody_.c_str());}
  else{++failures_;status_=Status::Retry;pending_=true;nextAt_=millis+std::min<uint32_t>(300000,10000u<<(std::min<uint32_t>(failures_,5)-1));}
  std::fill(responseBody_.begin(),responseBody_.end(),'\0');responseBody_.clear();printStatus();
}
void SimNetwork::tick(application::ProductController& product,const Clock& clock,int battery,bool allowed,uint32_t now){
  if(working_&&http_.done())finish(product,clock.unixSeconds,now);
  if(identityPending_){
    if(nextAt_&&int32_t(now-nextAt_)<0)return;
    Settings next=settings_;next.deviceId=claim_.deviceId;std::strcpy(next.uuid,claim_.uuid.data());next.claimUncertain=false;
    if(saveSettings(next)){identityPending_=false;pending_=true;halted_=false;status_=Status::Ready;claim_={};console_.printf("API PAIRED uuid=%s\n",settings_.uuid);}
    else status_=Status::StorageError;
    nextAt_=now+5000;return;
  }
  if(!allowed||working_||halted_||product.faulted())return;
  if(!storeReady_){status_=Status::StorageError;return;}
  if(settings_.endpoint.empty()){status_=Status::Unconfigured;return;}
  if(settings_.claimUncertain){status_=Status::ClaimUncertain;return;}
  const bool claim=pairingCode_[0]!=0;
  if(!claim&&!settings_.uuid[0]){status_=Status::Unpaired;return;}
  if(!clock.connected){status_=Status::WaitingNetwork;return;}
  if(!clock.known){status_=Status::WaitingClock;return;}
  if(!claim&&battery<0){status_=Status::BatteryUnknown;return;}
  if(nextAt_&&int32_t(now-nextAt_)<0){
    if(!(product.state().generation!=generation_&&uint32_t(now-lastAttempt_)>=2000&&failures_==0))return;
  }
  if(pending_||claim||product.state().generation!=generation_||!nextAt_||int32_t(now-nextAt_)>=0){
    if(!start(claim,product.state(),clock.unixSeconds,battery,now)&&!halted_){status_=Status::Retry;pending_=true;lastAttempt_=now;generation_=product.state().generation;nextAt_=now+30000;}
  }
}
}
