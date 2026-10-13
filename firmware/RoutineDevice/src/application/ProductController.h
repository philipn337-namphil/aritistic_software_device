#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\ProductController.h"
#pragma once
#include "ProductState.h"
#include "ProductView.h"
#include "../input/Intent.h"
#include "DeviceApi.h"

namespace routine::application {
class ProductController {
 public:
  explicit ProductController(ProductStore& store) : store_(store) {}
  LoadResult begin();
  ImportResult importSchedule(const Schedule& schedule);
  api::Result applySync(const api::SyncResponse& response,const api::SyncRequest& request);
  bool retireAcknowledgedDays(int64_t today);
  void tick(domain::TimeSample time);
  void input(input::Intent intent, domain::TimeSample time);
  const ProductState& state() const { return state_; }
  const ProductView& view() const { return view_; }
  bool faulted() const { return fault_; }
  bool takeAlert() { const bool result = alert_; alert_ = false; return result; }
  uint32_t revision() const { return viewRevision_; }
  uint32_t sleepSeconds(domain::TimeSample now, uint32_t maximum = 300) const;
 private:
  bool commit();
  bool transition(size_t index, const domain::Transition& change);
  void show(Screen screen, int index = -1);
  void reconcile();
  // Sync/compaction preserve a browsing or confirmation screen when its
  // actual target is unchanged. Routine indices may move during compaction.
  bool canPreserveView(const ProductState& next,int& nextIndex) const;
  bool commitExternal();
  void fail();
  int currentRoutine() const;
  void presentReward(int index, int step = -1);
  void updateSummary(domain::TimeSample time);
  bool migrateRewards();
  bool feedReward();
  // ProductInput.cpp routes already validated input by screen family.
  void moveChoice(input::Intent intent);
  void handleHomeInput(input::Intent intent);
  void handleMenuInput(input::Intent intent);
  void handleCatalogInput(input::Intent intent);
  void handleCharacterInput(input::Intent intent);
  void handleRewardInput(input::Intent intent);
  void handleGrowthInput(input::Intent intent);
  void handleSummaryInput(input::Intent intent);
  void showSummaryPage(uint8_t page);
  void handleRoutineInput(input::Intent intent, domain::TimeSample time);
  ProductStore& store_;
  // Scratch state is a member so ESP32's small loop-task stack never holds snapshots.
  ProductState state_{}, candidate_{};
  ProductView view_{};
  bool fault_ = false, alert_ = false;
  domain::TimeSample now_{0,false};
  uint32_t viewRevision_ = 0;
};
}
