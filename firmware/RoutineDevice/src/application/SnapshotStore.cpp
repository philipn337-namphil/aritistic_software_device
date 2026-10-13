#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\SnapshotStore.cpp"
#include "SnapshotStore.h"
#include <cstring>

namespace routine::application {
LoadResult SnapshotStore::load(ProductState& output) {
  writable_ = false; current_ = -1; generation_ = 0;
  uint64_t floor = 0;
  if (!io_.floor(floor)) return LoadResult::IoError;
  bool damaged = false, unknown = false, ioError = false;
  for (unsigned i = 0; i < 2; ++i) {
    size_t size = 0; const auto read = io_.read(i, bytes_, sizeof(bytes_), size);
    if (read == SlotRead::Missing) continue;
    if (read == SlotRead::Error) { ioError = true; continue; }
    const auto decoded = decodeState(bytes_, size, scratch_);
    if (decoded == DecodeResult::Unsupported) { unknown = true; continue; }
    if (decoded != DecodeResult::Ok || !scratch_.generation) { damaged = true; continue; }
    if (current_ >= 0 && scratch_.generation == generation_) { damaged = true; continue; }
    if (current_ < 0 || scratch_.generation > generation_) { output = scratch_; generation_ = output.generation; current_ = int(i); }
  }
  if (unknown) return LoadResult::Unsupported;
  if (ioError) return LoadResult::IoError;
  if (generation_ < floor) return LoadResult::Corrupt;
  if (damaged) return current_ < 0 ? LoadResult::Corrupt : LoadResult::Recovered;
  if (generation_ > floor && !io_.seal(generation_)) return LoadResult::IoError;
  writable_ = true;
  if (current_ < 0) { output = {}; return LoadResult::Empty; }
  return LoadResult::Loaded;
}
bool SnapshotStore::save(const ProductState& state) {
  if (!writable_ || generation_ == UINT64_MAX || state.generation != generation_ + 1) return false;
  const size_t size = encodeState(state, bytes_, sizeof(bytes_)); if (!size) return false;
  const unsigned slot = current_ == 0 ? 1 : 0;
  // A failed write may nevertheless have reached the flash: latch read-only.
  writable_ = false;
  if (!io_.write(slot, bytes_, size)) return false;
  size_t readSize = 0;
  if (io_.read(slot, verify_, sizeof(verify_), readSize) != SlotRead::Ok || readSize != size || std::memcmp(bytes_, verify_, size)) return false;
  if (!io_.seal(state.generation)) return false;
  current_ = int(slot); generation_ = state.generation; writable_ = true;
  return true;
}
}
