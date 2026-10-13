#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\ScheduleTransfer.h"
#pragma once
#include "StateCodec.h"
#include <cstring>

namespace routine::application {
enum class TransferResult { Other, Invalid, Begun, Chunk, Cancelled, Complete };
// Bounded USB development transport. This is not a server API or JSON contract.
class ScheduleTransfer {
 public:
  TransferResult receive(const char* command, uint32_t now) {
    if (std::strncmp(command,"load ",5)) return TransferResult::Other;
    if (!std::strcmp(command,"load cancel")) { cancel(); return TransferResult::Cancelled; }
    if (!std::strncmp(command,"load begin ",11)) {
      const char* p = command+11; size_t length;
      if (!number(p,length) || *p || length < 29 || length > sizeof(bytes_)) return invalid();
      total_ = length; received_ = 0; active_ = true; touched_ = now; return TransferResult::Begun;
    }
    if (!active_ || uint32_t(now-touched_) >= 30000) return invalid();
    if (!std::strcmp(command,"load commit")) {
      if (received_ != total_) return invalid();
      active_ = false; return TransferResult::Complete;
    }
    if (std::strncmp(command,"load data ",10)) return invalid();
    const char* p = command+10; size_t offset;
    if (!number(p,offset) || *p++ != ' ' || offset != received_) return invalid();
    const size_t chars = std::strlen(p);
    if (!chars || chars > 128 || chars % 2 || chars/2 > total_-received_) return invalid();
    for (size_t i=0;i<chars;i+=2) {
      const int a=hex(p[i]),b=hex(p[i+1]); if(a<0||b<0)return invalid();
      bytes_[received_++] = uint8_t((a<<4)|b);
    }
    touched_=now;return TransferResult::Chunk;
  }
  void tick(uint32_t now) { if(active_ && uint32_t(now-touched_)>=30000)cancel(); }
  bool active() const { return active_; }
  const uint8_t* bytes() const { return bytes_; }
  size_t size() const { return total_; }
  size_t received() const { return received_; }
 private:
  void cancel() { active_=false;received_=total_=0; }
  TransferResult invalid() { cancel();return TransferResult::Invalid; }
  static int hex(char c) { return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1; }
  static bool number(const char*& p,size_t& value) {
    value=0;unsigned n=0;
    while(*p>='0'&&*p<='9'){if(++n>5)return false;value=value*10+unsigned(*p++-'0');}
    return n!=0;
  }
  uint8_t bytes_[kSnapshotBytes]{};
  size_t received_=0,total_=0;
  uint32_t touched_=0;
  bool active_=false;
};
}
