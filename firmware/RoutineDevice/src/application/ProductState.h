#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\ProductState.h"
#pragma once
#include "../domain/RoutineEngine.h"
#include "../domain/Companion.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace routine::application {
constexpr size_t kMaxRuns = 16;
constexpr size_t kTextBytes = 96;
constexpr size_t kMaxEvents = 1024;
using Text = std::array<char, kTextBytes>;
struct RoutineRecord {
  domain::RunState run{};
  Text title{};
  std::array<Text, domain::kMaxSteps> stepNames{};
  uint16_t acknowledgedRewards = 0;  // UI receipt only; never consumes earned rights.
  bool resultSeen = false;
  std::array<uint8_t,domain::kMaxSteps> food{};
  uint16_t consumedRewards = 0;
  uint16_t syncedRewards = 0; // Read-back DONE with the same completion timestamp.
  bool apiManaged = false;
};
struct ProductState {
  uint64_t generation = 0;
  uint64_t scheduleRevision = 0;
  uint8_t count = 0;
  std::array<RoutineRecord, kMaxRuns> routines{};
  uint16_t eventCount = 0;
  std::array<domain::Event, kMaxEvents> events{};
  domain::Companion companion{};
  uint32_t randomState = 0x6d2b79f5;
  int64_t summarySeenDay = -1;
  std::array<uint32_t,31> archivedFood{};
  int64_t retiredBeforeDay = -1; // Never re-import a day already durably archived.
};
enum class ImportResult { Accepted, Duplicate, Invalid, Conflict, Capacity, StorageError };
bool validText(const char* text, size_t capacity = kTextBytes);
bool validProduct(const ProductState& state);
struct DailyRoutineProgress { uint8_t ordinal = 0; uint8_t total = 0; };
// Product dates are KST (API contract); count only the selected run's start day.
DailyRoutineProgress dailyRoutineProgress(const ProductState& state, size_t index);
struct FoodInventory { uint32_t acquired = 0; uint16_t available = 0; };
FoodInventory foodInventory(const ProductState& state, uint8_t food);
// First-setup USB packet; live API updates use DeviceApi/SyncResponse instead.
struct Schedule {
  uint64_t revision = 0;
  uint8_t count = 0;
  std::array<RoutineRecord, kMaxRuns> routines{};
};
enum class LoadResult { Empty, Loaded, Recovered, Corrupt, Unsupported, IoError };
class ProductStore {
 public:
  virtual ~ProductStore() = default;
  virtual LoadResult load(ProductState& state) = 0;
  virtual bool save(const ProductState& state) = 0;
  // Archive the complete snapshot before retirement. Default preserves all data.
  virtual bool archive(const ProductState&) { return false; }
};
}
