#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\FlashSlots.cpp"
#include "FlashSlots.h"
#include "../FileArchive.h"
#include <FFat.h>
#include <esp_partition.h>
#include <cstdio>
#include <cerrno>
#include <unistd.h>
#include <cstring>
#include <algorithm>

namespace routine::platform {
namespace {
bool blankPartition() {
  const auto* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_FAT, "ffat");
  if (!partition) return false;
  uint8_t block[256];
  for (size_t offset = 0; offset < partition->size; offset += sizeof(block)) {
    if (esp_partition_read(partition, offset, block, sizeof(block)) != ESP_OK) return false;
    for (auto byte : block) if (byte != 0xff) return false;
    if (!(offset % 65536)) delay(1);
  }
  return true;
}
}
bool FlashSlots::begin() {
  markerReady_ = marker_.begin("routine-store",false);
  if (!markerReady_) return false;
  const bool committed = marker_.isKey("generation");
  bool initializing = !committed && marker_.getBool("initializing",false);
  // Even a failed FAT mount initializes wear-levelling metadata. Check blank
  // flash BEFORE mounting; remember ownership if first-boot formatting is cut.
  if (!committed && !initializing && blankPartition()) {
    if (marker_.putBool("initializing",true) != 1) return false;
    initializing = true;
  }
  mounted_ = FFat.begin(false);
  if (!mounted_ && initializing) mounted_ = FFat.format() && FFat.begin(false);
  return mounted_;
}
bool FlashSlots::floor(uint64_t& generation) {
  if (!markerReady_) return false;
  if (!marker_.isKey("generation")) { generation = 0; return true; }
  if (marker_.getType("generation") != PT_U64) return false;
  generation = marker_.getULong64("generation",0);
  return generation != 0;
}
bool FlashSlots::seal(uint64_t generation) {
  if (!markerReady_ || !generation || marker_.putULong64("generation",generation) != sizeof(generation)) return false;
  marker_.remove("initializing"); // A generation marker always forbids reformatting.
  return true;
}
application::SlotRead FlashSlots::read(unsigned slot, uint8_t* bytes, size_t capacity, size_t& size) {
  using application::SlotRead;
  size = 0; if (!mounted_ || slot > 1) return SlotRead::Error;
  FILE* f = std::fopen(slot ? "/ffat/state-b.bin" : "/ffat/state-a.bin", "rb");
  if (!f) return errno == ENOENT ? SlotRead::Missing : SlotRead::Error;
  size = std::fread(bytes, 1, capacity, f);
  const bool extra = std::fgetc(f) != EOF;
  const bool error = std::ferror(f) != 0;
  const bool closed = std::fclose(f) == 0;
  return !extra && !error && closed ? SlotRead::Ok : SlotRead::Error;
}
bool FlashSlots::write(unsigned slot, const uint8_t* bytes, size_t size) {
  if (!mounted_ || slot > 1) return false;
  FILE* f = std::fopen(slot ? "/ffat/state-b.bin" : "/ffat/state-a.bin", "wb");
  if (!f) return false;
  bool ok = std::fwrite(bytes, 1, size, f) == size;
  ok = std::fflush(f) == 0 && ok;
  ok = ::fsync(::fileno(f)) == 0 && ok;
  return std::fclose(f) == 0 && ok;
}
bool FlashSlots::archive(uint64_t generation,const uint8_t* bytes,size_t size) {
  if(!mounted_||!generation||!bytes||!size)return false;
  char path[80];std::snprintf(path,sizeof(path),"/ffat/history-%llu.bin",static_cast<unsigned long long>(generation));
  char temporary[80];std::snprintf(temporary,sizeof(temporary),"/ffat/history-%llu.tmp",static_cast<unsigned long long>(generation));
  const auto result=writeArchive(path,temporary,bytes,size,[](FILE* file){return ::fsync(::fileno(file))==0;});
  return result==ArchiveResult::Saved||result==ArchiveResult::Existing;
}
}
