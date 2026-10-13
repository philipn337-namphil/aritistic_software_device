#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\ProductSync.cpp"
#include "ProductController.h"
#include "DailySummary.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace routine::application {
namespace {
bool sameRun(const RoutineRecord& a,const RoutineRecord& b){
  const auto& x=a.run.spec;const auto& y=b.run.spec;
  if(x.id!=y.id||x.startsAt!=y.startsAt||x.endsAt!=y.endsAt||x.stepCount!=y.stepCount||std::strcmp(a.title.data(),b.title.data()))return false;
  for(size_t i=0;i<x.stepCount;++i)if(x.steps[i]!=y.steps[i]||std::strcmp(a.stepNames[i].data(),b.stepNames[i].data()))return false;
  return true;
}
bool requested(const api::SyncRequest& request,int64_t day){for(size_t i=0;i<request.dateCount;++i)if(request.dates[i]==day)return true;return false;}
bool echoed(const api::SyncRequest& request,uint64_t run,uint32_t step,int64_t when){for(size_t i=0;i<request.completionCount;++i){const auto& c=request.completions[i];if(c.run==run&&c.step==step&&c.at==when)return true;}return false;}
const RoutineRecord* recordById(const ProductState& state,domain::RunId id,int* index=nullptr){
  for(size_t i=0;i<state.count;++i)if(state.routines[i].run.spec.id==id){if(index)*index=int(i);return &state.routines[i];}
  return nullptr;
}
bool sameExecution(const RoutineRecord& a,const RoutineRecord& b){
  return sameRun(a,b)&&a.run.version==b.run.version&&a.run.phase==b.run.phase&&
    domain::completedMask(a.run)==domain::completedMask(b.run)&&a.run.earnedRewards==b.run.earnedRewards&&
    a.acknowledgedRewards==b.acknowledgedRewards&&a.consumedRewards==b.consumedRewards&&a.resultSeen==b.resultSeen;
}
}
bool ProductController::canPreserveView(const ProductState& next,int& nextIndex) const {
  nextIndex=view_.routine;
  // A newly active/reward-bearing run must interrupt an unrelated screen. A
  // future definition or an ACK bit alone has no such presentation meaning.
  for(size_t i=0;i<next.count;++i){const auto& incoming=next.routines[i];
    if(incoming.run.phase==domain::Phase::Scheduled)continue;
    const auto* prior=recordById(state_,incoming.run.spec.id);
    if(!prior||!sameExecution(*prior,incoming))return false;
  }
  if(view_.routine>=0){
    if(view_.routine>=state_.count)return false;
    const auto& prior=state_.routines[size_t(view_.routine)];
    const auto* incoming=recordById(next,prior.run.spec.id,&nextIndex);
    if(!incoming||!sameExecution(prior,*incoming))return false;
  }
  // A summary must be reconstructed if its day's totals changed.
  if(isSummaryScreen(view_.screen)){
    const auto before=summarizeDay(state_,view_.summary.day),after=summarizeDay(next,view_.summary.day);
    if(before.runs!=after.runs||before.steps!=after.steps||before.completedSteps!=after.completedSteps||
       before.completedRuns!=after.completedRuns||before.abandonedRuns!=after.abandonedRuns||before.expiredRuns!=after.expiredRuns)return false;
  }
  return true;
}
bool ProductController::commitExternal(){
  int index=-1;const bool preserve=canPreserveView(candidate_,index);
  if(!commit())return false;
  updateSummary(now_);
  if(preserve)view_.routine=static_cast<int8_t>(index);else reconcile();
  return true;
}
api::Result ProductController::applySync(const api::SyncResponse& response,const api::SyncRequest& request){
  if(fault_)return api::Result::StorageError;
  if(!request.dateCount||request.dateCount>api::kMaxDates||response.dateCount>api::kMaxDates||response.count>kMaxRuns||request.completionCount>api::kMaxCompletions||response.serverTime<1577836800)return api::Result::InvalidField;
  for(size_t i=0;i<response.dateCount;++i)if(!requested(request,response.dates[i]))return api::Result::InvalidField;
  candidate_=state_;bool changed=false;
  for(size_t n=0;n<response.count;++n){
    const auto& remote=response.routines[n];const auto& incoming=remote.record;
    const auto day=koreanDay(incoming.run.spec.startsAt);
    if(!incoming.apiManaged||!domain::valid(incoming.run)||!requested(request,day))return api::Result::InvalidField;
    if(day<state_.retiredBeforeDay)continue;
    size_t index=0;while(index<candidate_.count&&candidate_.routines[index].run.spec.id!=incoming.run.spec.id)++index;
    if(index==candidate_.count){if(candidate_.count==kMaxRuns)return api::Result::Capacity;candidate_.routines[index]=incoming;++candidate_.count;changed=true;}
    else {
      auto& local=candidate_.routines[index];
      if(!local.apiManaged)return api::Result::Conflict; // USB IDs are a separate origin.
      if(!sameRun(local,incoming)){
        // An untouched future schedule can be replaced without losing outcomes.
        if(local.run.phase!=domain::Phase::Scheduled||domain::completedMask(local.run)||!now_.trusted||now_.unixSeconds>=local.run.spec.startsAt)return api::Result::Conflict;
        local=incoming;changed=true;
      }
    }
    auto& local=candidate_.routines[index];auto& run=local.run;
    const auto before=domain::completedMask(run);
    // DONE is monotonic locally. PENDING cannot revoke earned rewards or a
    // previously observed DONE without a server revision/conflict contract.
    run.remoteCompleted|=incoming.run.remoteCompleted;
    for(size_t s=0;s<run.spec.stepCount;++s){
      const uint16_t bit=uint16_t(1u<<s);
      if((run.earnedRewards&bit)&&(incoming.run.remoteCompleted&bit)&&!(local.syncedRewards&bit)&&echoed(request,run.spec.id,run.spec.steps[s],remote.completedAt[s])){
        bool localEvent=false;for(size_t e=0;e<candidate_.eventCount;++e){const auto& event=candidate_.events[e];if(event.kind==domain::EventKind::StepCompleted&&event.run==run.spec.id&&event.step==run.spec.steps[s]&&event.occurredAt==remote.completedAt[s])localEvent=true;}
        if(localEvent){local.syncedRewards|=bit;changed=true;}
      }
    }
    // Overlapping bits need not be represented twice; local rewards are authority
    // for their own origin and remain backed by their immutable completion event.
    run.remoteCompleted&=uint16_t(~run.earnedRewards);
    if(before!=domain::completedMask(run)){
      if(run.version==UINT32_MAX)return api::Result::Conflict;
      run.completedSteps=domain::countSteps(domain::completedMask(run));
      if(run.completedSteps==run.spec.stepCount){run.phase=domain::Phase::Completed;local.resultSeen=true;}
      else if(run.phase!=domain::Phase::Scheduled){++run.version;run.lastTransitionAt=std::max(run.lastTransitionAt,response.serverTime);}
      if(run.phase==domain::Phase::Completed){++run.version;run.lastTransitionAt=std::max({run.lastTransitionAt,response.serverTime,run.spec.startsAt});}
      changed=true;
    }
  }
  // No deletion by omission: without revision/tombstones, keep any locally
  // present run. Report structural edits instead of erasing an in-progress run.
  for(size_t i=0;i<candidate_.count;++i){const auto& a=candidate_.routines[i].run;for(size_t j=0;j<i;++j){const auto& b=candidate_.routines[j].run;
    if(!domain::terminal(a.phase)&&!domain::terminal(b.phase)&&domain::executionWindowsOverlap(a,b))return api::Result::Conflict;
    for(size_t s=0;s<a.spec.stepCount;++s)for(size_t t=0;t<b.spec.stepCount;++t)if(a.spec.steps[s]==b.spec.steps[t])return api::Result::Conflict;
  }}
  if(!changed)return api::Result::Ok;
  if(candidate_.scheduleRevision==UINT64_MAX)return api::Result::Conflict;
  ++candidate_.scheduleRevision;
  if(!validProduct(candidate_))return api::Result::InvalidField;
  return commitExternal()?api::Result::Ok:api::Result::StorageError;
}
bool ProductController::retireAcknowledgedDays(int64_t today){
  if(fault_||today<2)return false;
  int64_t cutoff=today-1; // Keep yesterday available for summaries and inspection.
  for(size_t i=0;i<state_.count;++i){const auto& r=state_.routines[i];const auto day=koreanDay(r.run.spec.startsAt);
    if(day>=cutoff)continue;
    if(!r.apiManaged||!domain::terminal(r.run.phase)||!r.resultSeen||r.consumedRewards!=r.run.earnedRewards||r.syncedRewards!=r.run.earnedRewards)cutoff=day;
  }
  if(cutoff<=state_.retiredBeforeDay)return false;
  bool retiring=false;for(size_t i=0;i<state_.count;++i)if(koreanDay(state_.routines[i].run.spec.startsAt)<cutoff)retiring=true;
  if(!retiring)return false;
  candidate_=state_;candidate_.count=0;candidate_.eventCount=0;
  for(size_t i=0;i<state_.count;++i){const auto& r=state_.routines[i];if(koreanDay(r.run.spec.startsAt)>=cutoff){candidate_.routines[candidate_.count++]=r;continue;}
    for(size_t s=0;s<r.run.spec.stepCount;++s)if(r.food[s]){auto& count=candidate_.archivedFood[r.food[s]-1];if(count==UINT32_MAX)return false;++count;}
  }
  for(size_t i=0;i<state_.eventCount;++i){const auto& e=state_.events[i];for(size_t j=0;j<candidate_.count;++j)if(candidate_.routines[j].run.spec.id==e.run)candidate_.events[candidate_.eventCount++]=e;}
  candidate_.retiredBeforeDay=cutoff;
  // Archive first, then publish compaction in the same snapshot as lifetime food.
  // A crash in between only leaves an extra immutable archive, never lost data.
  if(!store_.archive(state_))return false;
  return commitExternal();
}
}
