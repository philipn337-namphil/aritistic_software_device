#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\DeviceApi.h"
#pragma once
#include "ProductState.h"
#include <array>
#include <string>

namespace routine::api {
constexpr size_t kMaxDates=3, kMaxCompletions=64, kMaxJsonBytes=98304;
struct RemoteRoutine {
  application::RoutineRecord record{};
  std::array<int64_t,domain::kMaxSteps> completedAt{};
  int order=0;
};
struct SyncResponse {
  int64_t serverTime=0;
  uint32_t accepted=0;
  uint8_t count=0, dateCount=0;
  std::array<int64_t,kMaxDates> dates{};
  std::array<RemoteRoutine,application::kMaxRuns> routines{};
};
struct Completion { uint64_t run=0; uint32_t step=0; int64_t at=0; };
struct SyncRequest {
  int battery=-1;
  uint8_t dateCount=0, completionCount=0;
  std::array<int64_t,kMaxDates> dates{};
  std::array<Completion,kMaxCompletions> completions{};
};
struct ClaimResponse { uint64_t deviceId=0; std::array<char,37> uuid{}; int64_t serverTime=0; };
enum class Result { Ok, InvalidJson, InvalidEnvelope, InvalidField, Capacity, Conflict, StorageError };
bool parseDate(const char* text,int64_t& day);
bool parseTimestamp(const char* text,int64_t& timestamp);
std::string formatDate(int64_t day);
std::string formatTimestamp(int64_t timestamp);
bool validUuid(const char* value);
bool validPairingCode(const char* value);
Result parseSync(const char* data,size_t size,SyncResponse& out,
                 domain::Policy policy={domain::DeadlineRule::DeadlineWins,0});
Result parseClaim(const char* data,size_t size,ClaimResponse& out);
bool makeClaim(const char* code,const char* deviceUid,const char* firmware,std::string& out);
bool makeSync(const SyncRequest& request,const char* firmware,std::string& out);
// Only snapshot reads; asynchronous HTTP must never hold a mutable controller.
bool prepareSync(const application::ProductState& state,int64_t now,int battery,SyncRequest& out);
}
