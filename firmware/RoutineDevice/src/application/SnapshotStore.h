#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\SnapshotStore.h"
#pragma once
#include "StateCodec.h"

namespace routine::application {
enum class SlotRead { Missing, Ok, Error };
struct SlotIo {
  virtual ~SlotIo() = default;
  virtual SlotRead read(unsigned slot, uint8_t* bytes, size_t capacity, size_t& size) = 0;
  // Return only after close/fsync. Store verifies the entire written image again.
  virtual bool write(unsigned slot, const uint8_t* bytes, size_t size) = 0;
  // Independent durable high-water mark detects both files lost / older rollback.
  virtual bool floor(uint64_t& generation) = 0;
  virtual bool seal(uint64_t generation) = 0;
  virtual bool archive(uint64_t,const uint8_t*,size_t) { return false; }
};
class SnapshotStore : public ProductStore {
 public:
  explicit SnapshotStore(SlotIo& io) : io_(io) {}
  LoadResult load(ProductState& output) override;
  bool save(const ProductState& state) override;
  bool archive(const ProductState& state) override {
    const size_t size=encodeState(state,bytes_,sizeof(bytes_));
    return size && io_.archive(state.generation,bytes_,size);
  }
 private:
  SlotIo& io_;
  uint8_t bytes_[kSnapshotBytes]{}, verify_[kSnapshotBytes]{};
  ProductState scratch_{};
  int current_ = -1;
  uint64_t generation_ = 0;
  bool writable_ = false;
};
}
