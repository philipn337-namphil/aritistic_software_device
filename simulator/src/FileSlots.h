#pragma once
#include "../../firmware/RoutineDevice/src/application/SnapshotStore.h"
#include <string>

namespace routine::sim {
// Host-file replacement for platform/esp32/FlashSlots: the same A/B snapshot
// files plus a separate generation marker (the ESP32 keeps it in NVS).
class FileSlots : public application::SlotIo {
 public:
  explicit FileSlots(std::string directory):directory_(std::move(directory)){}
  application::SlotRead read(unsigned slot,uint8_t* bytes,size_t capacity,size_t& size) override;
  bool write(unsigned slot,const uint8_t* bytes,size_t size) override;
  bool floor(uint64_t& generation) override;
  bool seal(uint64_t generation) override;
  bool archive(uint64_t generation,const uint8_t* bytes,size_t size) override;
  const std::string& directory() const { return directory_; }
 private:
  std::string path(const char* name) const { return directory_+"/"+name; }
  std::string directory_;
};

// Tiny key=value file for what the ESP32 keeps in Preferences/NVS.
bool readSetting(const std::string& file,const char* key,std::string& value);
bool writeSetting(const std::string& file,const char* key,const std::string& value);
}
