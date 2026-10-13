#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\AssetStorage.h"
#pragma once
#include <Arduino.h>
#include <cstdio>
namespace routine::platform {
// A versioned UI file is separate from state-a/b and the NVS progress seal.
class AssetStorage {
 public:
  bool begin(bool mounted);
  bool command(const char* line);
  void tick();
  bool receiving() const { return chunkSize_!=0; }
  bool active() const { return file_!=nullptr; }
  bool ready() const { return memory_&&reader_&&!readError_; }
 private:
  bool load(const char* path,bool activate);
  static const uint8_t* readAsset(void* context,size_t offset,size_t size);
  void abort(const char* reason);
  void info();
  bool mounted_=false;
  uint8_t* memory_=nullptr;
  FILE* file_=nullptr;
  FILE* reader_=nullptr;
  size_t cacheOffset_[2]={SIZE_MAX,SIZE_MAX},cacheSize_[2]{};
  unsigned cacheNext_=0;
  bool readError_=false;
  uint32_t received_=0,chunkSize_=0,chunkAt_=0,chunkCrc_=0,lastAt_=0;
  uint8_t chunk_[4096];
};
}
