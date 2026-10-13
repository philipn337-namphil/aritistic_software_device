#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\WifiProvisioning.h"
#pragma once
#include <cstddef>
#include <cstdint>

namespace routine::provisioning {
inline constexpr char kServiceUuid[]="7f3a0001-5d2e-4c1b-9a6f-8e4b2c1d0a00";
inline constexpr char kConfigUuid[]="7f3a0002-5d2e-4c1b-9a6f-8e4b2c1d0a00";
inline constexpr char kStatusUuid[]="7f3a0003-5d2e-4c1b-9a6f-8e4b2c1d0a00";
void erase(void* data,size_t size);
struct Config {
  char ssid[33]{},password[64]{},pairingCode[11]{},serverUrl[192]{};
  void clear(){erase(this,sizeof(*this));}
};
// HTTP is accepted only for numeric RFC1918 IPv4 origins in an explicit dev build.
bool validServerUrl(const char* url,bool allowLocalHttp);
bool parseConfig(const char* bytes,size_t size,Config& out,bool allowLocalHttp);

enum class FrameResult { Waiting,Complete,Invalid };
class Frame {
 public:
  static constexpr size_t kCapacity=1024;
  static constexpr uint32_t kTimeoutMs=10000;
  ~Frame(){clear();}
  FrameResult push(const uint8_t* data,size_t size,uint32_t now);
  bool expire(uint32_t now);
  void clear();
  const char* bytes()const{return bytes_;}
  size_t size()const{return size_;}
 private:
  char bytes_[kCapacity+1]{};
  size_t size_=0;
  uint32_t lastAt_=0;
  bool dropping_=false,complete_=false;
};

enum class JoinState { Idle,Connecting,Committed,Failed };
class WifiPort {
 public:
  virtual ~WifiPort()=default;
  virtual bool tryCandidate(const char* ssid,const char* password)=0;
  virtual bool hasCandidateIp()const=0;
  virtual bool persistCandidate()=0;
  virtual void restorePrevious()=0;
};
class Join {
 public:
  static constexpr uint32_t kTimeoutMs=30000;
  bool start(WifiPort& port,const Config& config,uint32_t now);
  void tick(WifiPort& port,uint32_t now);
  void cancel(WifiPort& port);
  JoinState state()const{return state_;}
 private:
  uint32_t beganAt_=0;
  JoinState state_=JoinState::Idle;
};
}
