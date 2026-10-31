#include "SimRuntime.h"
#include "../../firmware/RoutineDevice/src/diagnostics/DeviceCommand.h"
#include "../../firmware/RoutineDevice/src/generated/AssetPack.h"
#include "../../firmware/RoutineDevice/src/presentation/Drawing.h"
#include "../../firmware/RoutineDevice/src/presentation/ImageArchive.h"
#include "../../firmware/RoutineDevice/src/presentation/ProductRenderer.h"
#include "../../firmware/RoutineDevice/src/Version.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <random>

namespace routine::sim {
namespace {
constexpr int kBatteryPercent = 100;  // Same fixed value as the development firmware.
}
uint32_t SimRuntime::millis(){
  static const auto start=std::chrono::steady_clock::now();
  return uint32_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count());
}
SimRuntime::SimRuntime(const std::string& dataDirectory,const std::string& assetPack)
  :directory_(dataDirectory),assetPack_(assetPack),pixels_(240*320,0),slots_(dataDirectory),store_(slots_),demo_(store_),product_(demo_),
   network_(console_,dataDirectory+"/api.txt"){}
int64_t SimRuntime::liveSeconds() const {
  return int64_t(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count())+clockOffset_;
}
// The PC clock plays the part of an NTP-synchronised ESP32 system clock.
domain::TimeSample SimRuntime::time() const { return demo_.active()?demo_.time(millis()):domain::TimeSample{liveSeconds(),true}; }
void SimRuntime::loadSettings(){
  std::string value;
  motion_.enable(!readSetting(directory_+"/ui.txt","motion",value)||value!="0");
}
bool SimRuntime::begin(){
  {std::ifstream file(assetPack_,std::ios::binary);assetBytes_.assign(std::istreambuf_iterator<char>(file),{});}
  assetsReady_=ui::bindImageArchive(assetBytes_.data(),assetBytes_.size(),assets::kAssetPayloadBytes,assets::kAssetCrc);
  auto loaded=product_.begin();
  // Persist the first empty ledger so even pre-configuration reboots exercise restoration.
  if(loaded==application::LoadResult::Empty){
    importScratch_={};importScratch_.generation=1;importScratch_.randomState=std::random_device{}()|1u;
    if(!store_.save(importScratch_))console_.println("ERR initial ledger save");
    loaded=product_.begin();
  }
  network_.begin(millis());loadSettings();armButton();activityAt_=millis();ready_=true;
  console_.printf("RoutineDevice VIRTUAL v%s data=%s load=%u assets=%d\n",kFirmwareVersion,directory_.c_str(),unsigned(loaded),assetsReady_);
  console_.println("Buttons: D4=Previous, D5=Confirm, D6=Next. help for commands.");
  status();
  return true;
}
void SimRuntime::status(){
  console_.printf("Session: demo=%d clock=%s persistence=%s\n",demo_.active(),demo_.active()?"simulated":"system",demo_.active()?"ram":"file");
  network_.printStatus();
  console_.printf("Product: generation=%llu revision=%llu runs=%u events=%u screen=%u selection=%u fault=%d\n",
    static_cast<unsigned long long>(product_.state().generation),static_cast<unsigned long long>(product_.state().scheduleRevision),
    unsigned(product_.state().count),unsigned(product_.state().eventCount),unsigned(product_.view().screen),unsigned(product_.view().selected),product_.faulted());
  console_.printf("Clock: offset=%llds wifi=%d; Power: idle=%lus sleeps=%lu screen=%s; motion=%d\n",static_cast<long long>(clockOffset_),wifi_,static_cast<unsigned long>(idleMs_/1000),
    static_cast<unsigned long>(sleepCount_),sleeping_?"off":"on",motion_.enabled());
  console_.printf("Input: context=%lu discarded=%lu\n",static_cast<unsigned long>(inputSession_.matches(product_.state(),product_.view())?inputSession_.context():0),static_cast<unsigned long>(discardedInputs_));
  const auto& companion=product_.state().companion;
  const auto growth=domain::growthView(companion.experience[companion.selected]);
  console_.printf("Companion: selected=%u stage=%u growth=%u/%u notice=%u offered=%u; item=%u\n",
    unsigned(companion.selected),unsigned(growth.stage),unsigned(growth.progress),unsigned(growth.target),
    unsigned(companion.notice),unsigned(companion.offered),unsigned(product_.view().item));
  if(demo_.active()){
    const auto& record=product_.state().routines[0];
    console_.printf("DEMO elapsed=%lld phase=%u completed=%u earned=%u consumed=%u warning=%d summary=%u/%u\n",
      static_cast<long long>(time().unixSeconds-diagnostics::DemoSession::kEpoch),unsigned(record.run.phase),unsigned(record.run.completedSteps),
      unsigned(record.run.earnedRewards),unsigned(record.consumedRewards),record.run.warningEmitted,
      unsigned(product_.view().summary.completedSteps),unsigned(product_.view().summary.steps));
  }
}
std::string SimRuntime::statusJson() const {
  const auto& state=product_.state();const auto& view=product_.view();
  char json[1024];
  std::snprintf(json,sizeof(json),
    "{\"version\":\"%s\",\"frames\":%lu,\"beeps\":%lu,\"screenOn\":%s,\"demo\":%s,\"wifi\":%s,\"motion\":%s,"
    "\"idleSeconds\":%lu,\"sleeps\":%lu,\"api\":\"%s\",\"http\":%d,\"paired\":%s,\"endpoint\":\"%s\",\"deviceUid\":\"%s\","
    "\"uuid\":\"%s\",\"generation\":%llu,\"scheduleRevision\":%llu,\"routines\":%u,\"events\":%u,\"screen\":%u,"
    "\"selected\":%u,\"fault\":%s,\"assets\":%s,\"unixSeconds\":%lld,\"clockOffset\":%lld,\"discarded\":%lu}",
    kFirmwareVersion,static_cast<unsigned long>(frames_),static_cast<unsigned long>(beeps_),sleeping_?"false":"true",
    demo_.active()?"true":"false",wifi_?"true":"false",motion_.enabled()?"true":"false",static_cast<unsigned long>(idleMs_/1000),
    static_cast<unsigned long>(sleepCount_),network_.statusName(),network_.httpStatus(),network_.paired()?"true":"false",network_.endpoint(),
    network_.deviceUid().c_str(),network_.uuid(),static_cast<unsigned long long>(state.generation),
    static_cast<unsigned long long>(state.scheduleRevision),unsigned(state.count),unsigned(state.eventCount),unsigned(view.screen),
    unsigned(view.selected),product_.faulted()?"true":"false",assetsReady_?"true":"false",static_cast<long long>(time().unixSeconds),
    static_cast<long long>(clockOffset_),static_cast<unsigned long>(discardedInputs_));
  return json;
}
void SimRuntime::armButton(){
  events_.clear();gestures_.begin(millis(),buttons_,inputContext_);
}
void SimRuntime::resetPresentation(){
  const bool enabled=motion_.enabled();motion_=ui::MotionPlayer{};motion_.enable(enabled);
  inputSession_.invalidate();inputContext_=0;armButton();dirty_=true;pendingSleep_=0;activityAt_=millis();
}
// The web page reports raw button levels over HTTP, so presses can arrive late
// or bunched together. Each press is held for at least kMinHoldMs of host time
// and the board's 5 ms sampling is replayed for any gap in the loop, so the
// firmware's 30 ms debounce sees the same levels a real finger would produce.
void SimRuntime::buttons(uint8_t mask){
  const uint32_t now=millis();
  pumpGestures(now);
  for(uint8_t bit=0;bit<3;++bit)
    if((mask&(1u<<bit))&&!(requested_&(1u<<bit))){pressedAt_[bit]=now;held_|=uint8_t(1u<<bit);}
  requested_=mask&7;
}
uint8_t SimRuntime::level(uint32_t at){
  for(uint8_t bit=0;bit<3;++bit)
    if((held_&(1u<<bit))&&!(requested_&(1u<<bit))&&uint32_t(at-pressedAt_[bit])>=kMinHoldMs)held_&=uint8_t(~(1u<<bit));
  held_|=requested_;
  return held_;
}
void SimRuntime::pumpGestures(uint32_t now){
  if(uint32_t(now-lastGesture_)>1000)lastGesture_=now-5;
  while(uint32_t(now-lastGesture_)>=5){
    lastGesture_+=5;
    buttons_=level(lastGesture_);
    const auto event=gestures_.update(lastGesture_,buttons_,inputContext_);
    if(event.intent!=input::Intent::None){if(events_.size()<16)events_.push_back(event);else armButton();}
  }
}
bool SimRuntime::demoCommand(const char* line){
  const auto parsed=diagnostics::parseDemoCommand(line);
  using Action=diagnostics::DemoAction;
  if(parsed.action==Action::Other)return false;
  if(parsed.action==Action::Status){status();return true;}
  if(parsed.action==Action::Invalid){console_.println("DEMO ERR command");return true;}
  if(parsed.action==Action::Start){
    if(network_.busy()||transfer_.active()||!assetsReady_||(!demo_.active()&&product_.faulted())){console_.println("DEMO ERR busy or unavailable");return true;}
    if(!demo_.active())liveMotionEnabled_=motion_.enabled();
    demo_.start(millis());product_.begin();demo_.schedule(schedule_);
    if(product_.importSchedule(schedule_)!=application::ImportResult::Accepted){demo_.stop();product_.begin();resetPresentation();console_.println("DEMO ERR schedule");return true;}
    product_.tick(time());resetPresentation();console_.println("DEMO STARTED ram-only; reboot or demo stop restores live records");
  }else if(!demo_.active()){console_.println("DEMO ERR inactive");return true;}
  else if(parsed.action==Action::Stop){
    demo_.stop();product_.begin();motion_.enable(liveMotionEnabled_);resetPresentation();console_.println("DEMO STOPPED live records restored");
  }else if(parsed.action==Action::Reload){
    product_.begin();product_.tick(time());resetPresentation();console_.println("DEMO RELOADED ram snapshot");
  }else if(parsed.action==Action::Advance){
    if(!demo_.advance(parsed.seconds,millis())){console_.println("DEMO ERR time range");return true;}
    product_.tick(time());console_.println("DEMO ADVANCED");
  }
  status();return true;
}
void SimRuntime::command(const char* line){
  console_.printf("> %s\n",line);
  activityAt_=millis();
  if(demoCommand(line))return;
  if(demo_.active()&&!diagnostics::demoAllowsCommand(line)){console_.println("DEMO ERR command unavailable; demo stop first");return;}
  if(network_.command(line,millis()))return;
  if(!std::strcmp(line,"motion on")||!std::strcmp(line,"motion off")){
    const bool enabled=!std::strcmp(line,"motion on");
    if(!demo_.active()&&!writeSetting(directory_+"/ui.txt","motion",enabled?"1":"0")){console_.println("ERR motion setting");return;}
    motion_.enable(enabled);dirty_=true;console_.println(enabled?"OK motion on":"OK motion off");return;
  }
  // Simulator-only controls for what the real board gets from Wi-Fi and NTP.
  if(!std::strcmp(line,"wifi on")||!std::strcmp(line,"wifi off")){
    wifi_=!std::strcmp(line,"wifi on");if(wifi_)network_.request();console_.println(wifi_?"OK wifi on":"OK wifi off");return;
  }
  if(!std::strncmp(line,"clock offset ",13)){
    char* end=nullptr;const long long seconds=std::strtoll(line+13,&end,10);
    if(!end||*end||seconds<-7*86400||seconds>7*86400){console_.println("ERR clock offset -604800..604800");return;}
    clockOffset_=seconds;dirty_=true;console_.printf("OK clock offset %llds\n",seconds);return;
  }
  if(!std::strcmp(line,"wake")){if(sleeping_)wakeAt_=millis();console_.println("OK wake");return;}
  using application::TransferResult;
  const auto transferred=transfer_.receive(line,millis());
  if(transferred!=TransferResult::Other){
    if(transferred==TransferResult::Complete){
      const auto decoded=application::decodeState(transfer_.bytes(),transfer_.size(),importScratch_);
      if(decoded!=application::DecodeResult::Ok||importScratch_.generation||importScratch_.eventCount){console_.println("LOAD ERR payload");return;}
      schedule_={};schedule_.revision=importScratch_.scheduleRevision;schedule_.count=importScratch_.count;schedule_.routines=importScratch_.routines;
      const auto result=product_.importSchedule(schedule_);
      console_.printf("LOAD RESULT %u\n",unsigned(result));
    }else if(transferred==TransferResult::Invalid)console_.println("LOAD ERR transfer");
    else console_.printf("LOAD OK %u\n",unsigned(transfer_.received()));
    return;
  }
  if(!std::strcmp(line,"help")){
    console_.println("status | idle off | idle 1..3600 | sleep 1..86400 | wake | reboot");
    console_.println("input previous | input next | input confirm | motion on | motion off");
    console_.println("api status | api sync | api pair <10 digits> | api reset-uuid | api endpoint <url>");
    console_.println("demo start | demo stop | demo status | demo advance 1..86400 | demo reload (RAM only)");
    console_.println("virtual only: wifi on | wifi off | clock offset <seconds>");return;
  }
  if(!std::strcmp(line,"reboot")){
    console_.println("OK reboot");
    if(demo_.active()){demo_.stop();motion_.enable(liveMotionEnabled_);}
    product_.begin();loadSettings();sleeping_=false;resetPresentation();network_.request();status();return;
  }
  if(!std::strncmp(line,"input ",6)){
    if(!assetsReady_){console_.println("ERR assets missing");return;}
    const char* name=line+6;
    const auto intent=!std::strcmp(name,"previous")?input::Intent::Previous:!std::strcmp(name,"next")?input::Intent::Next:!std::strcmp(name,"confirm")?input::Intent::Confirm:input::Intent::None;
    if(intent==input::Intent::None){console_.println("ERR input");return;}
    dispatchInput(intent);status();return;
  }
  auto parsed=diagnostics::parseDeviceCommand(line);
  using Kind=diagnostics::DeviceCommandKind;
  switch(parsed.kind){
    case Kind::Status:status();break;
    case Kind::Sync:network_.request();console_.println("OK sync");break;
    case Kind::Idle:idleMs_=parsed.seconds*1000;console_.println("OK idle");break;
    case Kind::Sleep:pendingSleep_=parsed.seconds;break;
    default:console_.println("ERR command");break;
  }
  parsed.clearSecrets();
}
void SimRuntime::dispatchInput(input::Intent intent){
  if(motion_.transient(millis())){motion_.skip();dirty_=true;return;}
  product_.input(intent,time());
}
void SimRuntime::sleep(uint32_t seconds){
  if(demo_.active()||!seconds||inputBusy()||!canPresent()||transfer_.active()||network_.busy()||!assetsReady_)return;
  console_.printf("Sleep: %lus\n",static_cast<unsigned long>(seconds));
  sleeping_=true;wakeAt_=millis()+seconds*1000;++sleepCount_;
}
void SimRuntime::tick(){
  if(!ready_)return;
  const uint32_t now=millis();
  buzzer_.update(now);
  if(sleeping_){
    // ESP32 light sleep: LCD off, RAM kept; any button or the timer wakes it.
    buttons_=level(now);lastGesture_=now;
    if(!buttons_&&int32_t(now-wakeAt_)<0)return;
    sleeping_=false;armButton();network_.request();activityAt_=now;dirty_=true;
    console_.printf("Wake: cause=%s\n",buttons_?"button":"timer");
  }
  pumpGestures(now);
  if(!demo_.active())network_.tick(product_,{wifi_,true,liveSeconds()},kBatteryPercent,!transfer_.active(),now);
  if(!inputSession_.matches(product_.state(),product_.view())){inputSession_.invalidate();inputContext_=0;}
  input::InputEvent event{};if(!events_.empty()){event=events_.front();events_.pop_front();}
  const auto intent=event.intent;
  if(inputBusy()||intent!=input::Intent::None)activityAt_=now;
  // Controller input itself advances time and rejects presses for changed screens.
  if(intent!=input::Intent::None&&assetsReady_&&inputSession_.accepts(event.context,product_.state(),product_.view())){
    // A delayed skip press from the last animation frame is still only a skip.
    if(inputSession_.transient()){motion_.skip();dirty_=true;product_.tick(time());}
    else dispatchInput(intent);
  }else {if(intent!=input::Intent::None)++discardedInputs_;product_.tick(time());}
  transfer_.tick(now);
  if(product_.takeAlert())beep(80);
  uint16_t minutes=0;const bool known=true;
  minutes=uint16_t(((time().unixSeconds+9*3600)/60)%1440);
  motion_.observe(product_.state(),product_.view(),now);const auto motion=motion_.sample(now);
  dirty_=dirty_||motion.kind!=shownMotion_||motion.frame!=shownFrame_;
  dirty_=dirty_||revision_!=product_.revision()||minutes_!=minutes||clockShown_!=known||wifiShown_!=wifi_;
  dirty_=dirty_||(assetsReady_&&!inputSession_.context());
  if(dirty_&&canPresent()){
    const bool transient=ui::isTransient(motion.kind);
    if(!assetsReady_||!inputSession_.matches(product_.state(),product_.view())||inputSession_.transient()!=transient){inputSession_.invalidate();inputContext_=0;}
    if(assetsReady_)ui::renderProduct(pixels_.data(),product_.state(),product_.view(),known,minutes,motion);
    else {
      std::fill(pixels_.begin(),pixels_.end(),uint16_t(0xffff));
      ui::drawing::text(pixels_.data(),"화면 자료를 확인해 주세요",120,148,18,0);
      ui::drawing::text(pixels_.data(),"ui.pak 없음",120,185,16,0);
    }
    if(assetsReady_)ui::renderDeviceStatus(pixels_.data(),wifi_,kBatteryPercent);
    if(demo_.active()){
      ui::drawing::rect(pixels_.data(),78,0,84,20,0xffe0);
      ui::drawing::text(pixels_.data(),"TEST",120,16,16,0);
    }
    present();dirty_=false;wifiShown_=wifi_;shownMotion_=motion.kind;shownFrame_=motion.frame;
    if(assetsReady_){inputSession_.publish(product_.state(),product_.view(),transient);inputContext_=inputSession_.context();}
    minutes_=minutes;clockShown_=known;revision_=product_.revision();
  }
  if(pendingSleep_){const auto seconds=pendingSleep_;pendingSleep_=0;sleep(product_.sleepSeconds(time(),seconds));}
  else if(idleMs_&&uint32_t(now-activityAt_)>=idleMs_)sleep(product_.sleepSeconds(time()));
}
}
