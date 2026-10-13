#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\application\\DeviceApi.cpp"
#include "DeviceApi.h"
#include "DailySummary.h"
#include "JsonInput.h"
#include "../vendor/ArduinoJson.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>

namespace routine::api {
namespace {
// Bound allocations as well as wire size and nesting. Failure publishes nothing.
class LimitedAllocator : public ArduinoJson::Allocator {
  struct alignas(std::max_align_t) Header { size_t size; };
  size_t used_=0;
 public:
  void* allocate(size_t n) override {
    if(n>262144-used_) return nullptr;
    auto* p=static_cast<Header*>(std::malloc(sizeof(Header)+n));
    if(!p)return nullptr;
    p->size=n;used_+=n;return p+1;
  }
  void deallocate(void* p) override {if(p){auto* h=static_cast<Header*>(p)-1;used_-=h->size;std::free(h);}}
  void* reallocate(void* p,size_t n) override {
    if(!p)return allocate(n);
    auto* h=static_cast<Header*>(p)-1;
    if(n>262144-used_+h->size)return nullptr;
    const auto old=h->size;
    auto* next=static_cast<Header*>(std::realloc(h,sizeof(Header)+n));
    if(!next)return nullptr;
    used_=used_-old+n;next->size=n;return next+1;
  }
};
bool number(JsonVariantConst v,uint64_t& n,uint64_t maximum) {
  if(!v.is<uint64_t>())return false;
  n=v.as<uint64_t>();return n>0&&n<=maximum;
}
bool title(JsonVariantConst v,application::Text& out) {
  if(!v.is<const char*>())return false;
  const auto s=v.as<JsonString>();if(!s.size()||s.size()>=out.size()||std::strlen(s.c_str())!=s.size())return false;
  std::memcpy(out.data(),s.c_str(),s.size()+1);return application::validText(out.data());
}
bool envelope(const JsonDocument& doc) {
  return doc.is<JsonObjectConst>()&&doc["success"].is<bool>()&&doc["success"].as<bool>()&&doc["data"].is<JsonObjectConst>()&&doc["error"].isNull();
}
int digits(const char* s,size_t n){int v=0;for(size_t i=0;i<n;++i){if(s[i]<'0'||s[i]>'9')return -1;v=v*10+s[i]-'0';}return v;}
int monthDays(int y,int m){const int n[]={31,28,31,30,31,30,31,31,30,31,30,31};return n[m-1]+(m==2&&y%4==0&&(y%100!=0||y%400==0));}
int64_t civilDays(int y,int m,int d){y-=m<=2;const int era=y/400;const unsigned year=unsigned(y-era*400);const unsigned doy=(153*(m+(m>2?-3:9))+2)/5+d-1;return int64_t(era)*146097+year*365+year/4-year/100+doy-719468;}
bool clockTime(const char* s,int& seconds){if(!s||std::strlen(s)!=5||s[2]!=':')return false;int h=digits(s,2),m=digits(s+3,2);if(h<0||h>23||m<0||m>59)return false;seconds=h*3600+m*60;return true;}
void addDate(SyncRequest& r,int64_t day){for(size_t i=0;i<r.dateCount;++i)if(r.dates[i]==day)return;if(r.dateCount<kMaxDates)r.dates[r.dateCount++]=day;}
}
bool parseDate(const char* s,int64_t& day){
  if(!s||std::strlen(s)!=10||s[4]!='-'||s[7]!='-')return false;
  int y=digits(s,4),m=digits(s+5,2),d=digits(s+8,2);
  if(y<1970||y>9999||m<1||m>12||d<1||d>monthDays(y,m))return false;
  day=civilDays(y,m,d);return true;
}
bool parseTimestamp(const char* s,int64_t& value){
  if(!s)return false;
  const auto len=std::strlen(s);if(len<20||len>30||s[10]!='T'||s[13]!=':'||s[16]!=':'||s[len-1]!='Z')return false;
  char date[11];std::memcpy(date,s,10);date[10]=0;int64_t day;
  if(!parseDate(date,day))return false;
  int h=digits(s+11,2),m=digits(s+14,2),sec=digits(s+17,2);
  if(h<0||h>23||m<0||m>59||sec<0||sec>59)return false;
  if(len!=20){if(s[19]!='.'||len<22)return false;for(size_t i=20;i<len-1;++i)if(s[i]<'0'||s[i]>'9')return false;}
  value=day*86400+h*3600+m*60+sec;return true;
}
std::string formatDate(int64_t day){
  if(day<0||day>2932896)return {};
  int64_t z=day+719468;const auto era=z/146097;const unsigned doe=unsigned(z-era*146097);
  const unsigned yoe=(doe-doe/1460+doe/36524-doe/146096)/365;int y=int(yoe)+int(era)*400;
  const unsigned doy=doe-(365*yoe+yoe/4-yoe/100);const unsigned mp=(5*doy+2)/153;
  const unsigned d=doy-(153*mp+2)/5+1;const int m=int(mp)+(mp<10?3:-9);y+=m<=2;
  char b[16];std::snprintf(b,sizeof(b),"%04d-%02d-%02u",y,m,d);return b;
}
std::string formatTimestamp(int64_t t){if(t<0)return {};const auto date=formatDate(t/86400);if(date.empty())return {};char b[32];std::snprintf(b,sizeof(b),"%sT%02u:%02u:%02uZ",date.c_str(),unsigned(t%86400/3600),unsigned(t%3600/60),unsigned(t%60));return b;}
bool validUuid(const char* s){if(!s||std::strlen(s)!=36)return false;for(size_t i=0;i<36;++i){if(i==8||i==13||i==18||i==23){if(s[i]!='-')return false;}else if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')||(s[i]>='A'&&s[i]<='F')))return false;}return true;}
bool validPairingCode(const char* s){if(!s||std::strlen(s)!=10)return false;for(size_t i=0;i<10;++i)if(s[i]<'0'||s[i]>'9')return false;return true;}
Result parseClaim(const char* data,size_t size,ClaimResponse& out){
  if(!data||!size||size>4096)return Result::Capacity;
  LimitedAllocator memory;JsonDocument doc(&memory);if(!readJson(doc,data,size,6))return Result::InvalidJson;
  if(!envelope(doc))return Result::InvalidEnvelope;
  ClaimResponse candidate;auto d=doc["data"].as<JsonObjectConst>();
  const char* uuid=jsonText(d["deviceAccessUuid"]);
  if(!number(d["deviceId"],candidate.deviceId,UINT64_MAX)||!validUuid(uuid))return Result::InvalidField;
  std::strcpy(candidate.uuid.data(),uuid);
  if(!d["serverTime"].isNull()&&!parseTimestamp(jsonText(d["serverTime"]),candidate.serverTime))return Result::InvalidField;
  out=candidate;return Result::Ok;
}
Result parseSync(const char* data,size_t size,SyncResponse& out,domain::Policy policy){
  // Caller owns scratch output; only publish through ProductController after Ok.
  out={};if(!data||!size||size>kMaxJsonBytes)return Result::Capacity;
  LimitedAllocator memory;JsonDocument doc(&memory);if(!readJson(doc,data,size,10))return Result::InvalidJson;
  if(!envelope(doc))return Result::InvalidEnvelope;
  auto d=doc["data"].as<JsonObjectConst>();
  if(!parseTimestamp(jsonText(d["serverTime"]),out.serverTime)||!d["accepted"].is<uint32_t>()||!d["routines"].is<JsonArrayConst>())return Result::InvalidField;
  out.accepted=d["accepted"].as<uint32_t>();
  for(auto date:d["routines"].as<JsonArrayConst>()){
    if(out.dateCount==kMaxDates)return Result::Capacity;
    int64_t day;if(!parseDate(jsonText(date["date"]),day)||!date["bigRoutines"].is<JsonArrayConst>())return Result::InvalidField;
    for(size_t i=0;i<out.dateCount;++i)if(out.dates[i]==day)return Result::InvalidField;
    out.dates[out.dateCount++]=day;
    for(auto big:date["bigRoutines"].as<JsonArrayConst>()){
      if(out.count==application::kMaxRuns)return Result::Capacity;
      auto& remote=out.routines[out.count];auto& record=remote.record;auto& run=record.run;record.apiManaged=true;
      int start,end;uint64_t id;
      if(!number(big["bigRoutineId"],id,UINT64_MAX)||!title(big["title"],record.title)||!clockTime(jsonText(big["startTime"]),start)||!clockTime(jsonText(big["endTime"]),end)||end<=start||!big["sortOrder"].is<int>()||!big["smallRoutines"].is<JsonArrayConst>())return Result::InvalidField;
      run.spec.id=id;remote.order=big["sortOrder"].as<int>();run.spec.startsAt=day*86400+start-9*3600;run.spec.endsAt=day*86400+end-9*3600;
      for(size_t i=0;i<out.count;++i)if(out.routines[i].record.run.spec.id==id)return Result::InvalidField;
      std::array<JsonVariantConst,domain::kMaxSteps> steps{};size_t count=0;
      for(auto small:big["smallRoutines"].as<JsonArrayConst>()) {if(count==steps.size())return Result::Capacity;if(!small["sortOrder"].is<int>())return Result::InvalidField;steps[count++]=small;}
      if(!count)return Result::InvalidField;
      std::sort(steps.begin(),steps.begin()+count,[](auto a,auto b){const int x=a["sortOrder"].template as<int>(),y=b["sortOrder"].template as<int>();return x!=y?x<y:a["smallRoutineId"].template as<uint64_t>()<b["smallRoutineId"].template as<uint64_t>();});
      run.spec.stepCount=uint8_t(count);run.policy=policy;
      for(size_t s=0;s<count;++s){auto small=steps[s];uint64_t sid;
        if(!number(small["smallRoutineId"],sid,UINT32_MAX)||!title(small["title"],record.stepNames[s])||!small["status"].is<const char*>())return Result::InvalidField;
        run.spec.steps[s]=uint32_t(sid);
        for(size_t i=0;i<=out.count;++i){const auto& prev=out.routines[i].record.run.spec;for(size_t j=0;j<(i==out.count?s:prev.stepCount);++j)if(prev.steps[j]==sid)return Result::InvalidField;}
        const char* status=jsonText(small["status"]);if(!status)return Result::InvalidField;
        if(!std::strcmp(status,"DONE")){if(!parseTimestamp(jsonText(small["completedAt"]),remote.completedAt[s])||remote.completedAt[s]>out.serverTime)return Result::InvalidField;run.remoteCompleted|=uint16_t(1u<<s);}
        else if(std::strcmp(status,"PENDING")||!small["completedAt"].isNull())return Result::InvalidField;
      }
      run.completedSteps=domain::countSteps(run.remoteCompleted);
      if(run.completedSteps==count){run.phase=domain::Phase::Completed;run.version=1;run.lastTransitionAt=std::max(out.serverTime,run.spec.startsAt);record.resultSeen=true;}
      if(!domain::valid(run))return Result::InvalidField;
      ++out.count;
    }
  }
  const auto before=[](const auto& a,const auto& b){if(a.record.run.spec.startsAt!=b.record.run.spec.startsAt)return a.record.run.spec.startsAt<b.record.run.spec.startsAt;if(a.order!=b.order)return a.order<b.order;return a.record.run.spec.id<b.record.run.spec.id;};
  // At most 16 records: selection sort avoids std::sort's nested ~4KiB heap
  // frames for this large aggregate on the ESP32's 8KiB application stack.
  for(size_t i=0;i<out.count;++i){size_t first=i;for(size_t j=i+1;j<out.count;++j)if(before(out.routines[j],out.routines[first]))first=j;
    if(first!=i){auto* a=reinterpret_cast<unsigned char*>(&out.routines[i]);auto* b=reinterpret_cast<unsigned char*>(&out.routines[first]);for(size_t byte=0;byte<sizeof(RemoteRoutine);++byte)std::swap(a[byte],b[byte]);}}
  return Result::Ok;
}
bool makeClaim(const char* code,const char* uid,const char* firmware,std::string& out){
  out.clear();if(!validPairingCode(code)||!uid||!*uid||std::strlen(uid)>96||!firmware||!*firmware)return false;
  JsonDocument doc;doc["pairingCode"]=code;doc["deviceUid"]=uid;doc["firmware"]=firmware;return !doc.overflowed()&&serializeJson(doc,out)>0;
}
bool makeSync(const SyncRequest& r,const char* firmware,std::string& out){
  out.clear();if(r.battery<0||r.battery>100||!firmware||!*firmware||!r.dateCount||r.dateCount>kMaxDates||r.completionCount>kMaxCompletions)return false;
  JsonDocument doc;doc["battery"]=r.battery;doc["firmware"]=firmware;auto dates=doc["dates"].to<JsonArray>();auto completions=doc["completions"].to<JsonArray>();
  for(size_t i=0;i<r.dateCount;++i){auto day=formatDate(r.dates[i]);if(day.empty())return false;dates.add(day);}
  for(size_t i=0;i<r.completionCount;++i){const auto& c=r.completions[i];auto when=formatTimestamp(c.at);if(!c.step||when.empty())return false;auto item=completions.add<JsonObject>();item["smallRoutineId"]=c.step;item["status"]="DONE";item["completedAt"]=when;}
  return !doc.overflowed()&&serializeJson(doc,out)>0;
}
bool prepareSync(const application::ProductState& state,int64_t now,int battery,SyncRequest& out){
  out={};out.battery=battery;if(now<1577836800||battery<0||battery>100)return false;
  // Today always remains visible; at most two backlog dates per request.
  addDate(out,application::koreanDay(now));
  for(size_t i=0;i<state.eventCount&&out.completionCount<kMaxCompletions;++i){const auto& e=state.events[i];if(e.kind!=domain::EventKind::StepCompleted)continue;
    for(size_t r=0;r<state.count;++r){const auto& rec=state.routines[r];if(!rec.apiManaged||rec.run.spec.id!=e.run)continue;
      for(size_t s=0;s<rec.run.spec.stepCount;++s)if(rec.run.spec.steps[s]==e.step&&!(rec.syncedRewards&(1u<<s))){const auto day=application::koreanDay(rec.run.spec.startsAt);bool included=false;for(size_t j=0;j<out.dateCount;++j)included=included||out.dates[j]==day;if(!included&&out.dateCount==kMaxDates)continue;addDate(out,day);out.completions[out.completionCount++]={e.run,e.step,e.occurredAt};}
    }
  }
  addDate(out,application::koreanDay(now)+1);return true;
}
}
