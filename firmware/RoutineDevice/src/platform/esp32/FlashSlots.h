#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\FlashSlots.h"
#pragma once
#include "../../application/SnapshotStore.h"
#include <Preferences.h>
namespace routine::platform {
class FlashSlots : public application::SlotIo {
 public:
  bool begin();
  application::SlotRead read(unsigned slot, uint8_t* bytes, size_t capacity, size_t& size) override;
  bool write(unsigned slot, const uint8_t* bytes, size_t size) override;
  bool floor(uint64_t& generation) override;
  bool seal(uint64_t generation) override;
  bool archive(uint64_t generation,const uint8_t* bytes,size_t size) override;
 private:
  bool mounted_ = false;
  Preferences marker_;
  bool markerReady_ = false;
};
}
