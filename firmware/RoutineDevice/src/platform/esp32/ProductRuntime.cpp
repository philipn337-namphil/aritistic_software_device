#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\ProductRuntime.cpp"
#include "ProductRuntime.h"
#include "PowerSession.h"
#include "UsbConsole.h"
#include "StorageConsole.h"
#include "../../diagnostics/DeviceCommand.h"
#include "../../presentation/ProductRenderer.h"
#include "../../presentation/Drawing.h"
#include "../../Version.h"
#include <algorithm>
#include <cstring>
#include <Preferences.h>
#include <WiFi.h>

namespace routine::platform {
namespace {
constexpr int kBatteryPercent = 100;
}

domain::TimeSample ProductRuntime::time() const { return demo_.active()?demo_.time(millis()):domain::TimeSample{clock_.unixSeconds(),clock_.sync().known()}; }
void ProductRuntime::begin() {
  beginUsbConsole();
  ready_=board_.begin(); if(!ready_){Serial.println("FATAL framebuffer");return;}
  board_.inputContext(0);
  const bool mounted=flash_.begin();
  assets_.begin(mounted);
  auto loaded=product_.begin();
  // Persist the first empty ledger so even pre-configuration reboots exercise restoration.
  if (loaded==application::LoadResult::Empty) {
    importScratch_={};importScratch_.generation=1;importScratch_.randomState=esp_random()|1u;
    if(!store_.save(importScratch_))Serial.println("ERR initial ledger save");
    loaded=product_.begin();
  }
  clock_.begin();network_.begin();board_.armButton();activityAt_=millis();
  setupRequested_=kBleDevelopment&&!clock_.configured();
  Preferences uiSettings;if(uiSettings.begin("routine-ui",true))motion_.enable(uiSettings.getBool("motion",true));
  Serial.printf("RoutineDevice PRODUCT v%s storage-mounted=%d load=%u assets=%d\n",kFirmwareVersion,mounted,unsigned(loaded),assets_.ready());
  Serial.println("D3 tap=Next; double=Previous; hold800=Confirm. help for USB commands.");
  if(kBleDevelopment)Serial.println("BLE DEVELOPMENT: plaintext setup; D3 hold5s=settings; hold800+release=Confirm; local HTTP enabled");
  status();
}
void ProductRuntime::status() {
  Serial.printf("BLE: development=%d active=%d state=%s\n",kBleDevelopment,ble_.active(),ble_.status());
  Serial.printf("Session: demo=%d clock=%s persistence=%s\n",demo_.active(),demo_.active()?"simulated":"system",demo_.active()?"ram":"flash");
  network_.printStatus();
  Serial.printf("Product: generation=%llu revision=%llu runs=%u events=%u screen=%u selection=%u fault=%d\n",
    static_cast<unsigned long long>(product_.state().generation),static_cast<unsigned long long>(product_.state().scheduleRevision),
    unsigned(product_.state().count),unsigned(product_.state().eventCount),unsigned(product_.view().screen),unsigned(product_.view().selected),product_.faulted());
  Serial.printf("Clock: configured=%d connected=%d known=%d fresh=%d requests=%lu successes=%lu\n",clock_.configured(),clock_.connected(),clock_.sync().known(),clock_.sync().fresh(),
    static_cast<unsigned long>(clock_.requestCount()),static_cast<unsigned long>(clock_.successCount()));
  const auto ip=WiFi.localIP(),gateway=WiFi.gatewayIP();
  Serial.printf("Wi-Fi: ip=%s gateway=%s rssi=%d\n",ip.toString().c_str(),gateway.toString().c_str(),WiFi.RSSI());
  Serial.printf("Power: idle=%lus last-error=%d wake-cause=%d; free-heap=%u\n",static_cast<unsigned long>(idleMs_/1000),lastSleep_.error,int(lastSleep_.cause),ESP.getFreeHeap());
  Serial.printf("Memory: free-psram=%u; motion=%d\n",ESP.getFreePsram(),motion_.enabled());
  Serial.printf("Input: context=%lu discarded=%lu\n",static_cast<unsigned long>(inputSession_.matches(product_.state(),product_.view())?inputSession_.context():0),static_cast<unsigned long>(discardedInputs_));
  const auto& companion=product_.state().companion;
  const auto growth=domain::growthView(companion.experience[companion.selected]);
  Serial.printf("Companion: selected=%u stage=%u growth=%u/%u notice=%u offered=%u; item=%u\n",
    unsigned(companion.selected),unsigned(growth.stage),unsigned(growth.progress),unsigned(growth.target),
    unsigned(companion.notice),unsigned(companion.offered),unsigned(product_.view().item));
  if(demo_.active()){
    const auto& record=product_.state().routines[0];
    Serial.printf("DEMO elapsed=%lld phase=%u completed=%u earned=%u consumed=%u warning=%d summary=%u/%u\n",
      static_cast<long long>(time().unixSeconds-diagnostics::DemoSession::kEpoch),unsigned(record.run.phase),unsigned(record.run.completedSteps),
      unsigned(record.run.earnedRewards),unsigned(record.consumedRewards),record.run.warningEmitted,
      unsigned(product_.view().summary.completedSteps),unsigned(product_.view().summary.steps));
  }
}
void ProductRuntime::resetPresentation(){
  const bool enabled=motion_.enabled();motion_=ui::MotionPlayer{};motion_.enable(enabled);
  inputSession_.invalidate();board_.inputContext(0);board_.armButton();dirty_=true;pendingSleep_=0;activityAt_=millis();
}
bool ProductRuntime::demoCommand(const char* line){
  const auto parsed=diagnostics::parseDemoCommand(line);
  using Action=diagnostics::DemoAction;
  if(parsed.action==Action::Other)return false;
  if(parsed.action==Action::Status){status();return true;}
  if(parsed.action==Action::Invalid){Serial.println("DEMO ERR command");return true;}
  if(parsed.action==Action::Start){
    if(network_.busy()||transfer_.active()||assets_.active()||!assets_.ready()||(!demo_.active()&&product_.faulted())){Serial.println("DEMO ERR busy or unavailable");return true;}
    if(!demo_.active())liveMotionEnabled_=motion_.enabled();
    demo_.start(millis());product_.begin();demo_.schedule(schedule_);
    if(product_.importSchedule(schedule_)!=application::ImportResult::Accepted){demo_.stop();product_.begin();resetPresentation();Serial.println("DEMO ERR schedule");return true;}
    product_.tick(time());resetPresentation();Serial.println("DEMO STARTED ram-only; reboot or demo stop restores live records");
  }else if(!demo_.active()){Serial.println("DEMO ERR inactive");return true;}
  else if(parsed.action==Action::Stop){
    demo_.stop();product_.begin();motion_.enable(liveMotionEnabled_);resetPresentation();Serial.println("DEMO STOPPED live records restored");
  }else if(parsed.action==Action::Reload){
    product_.begin();product_.tick(time());resetPresentation();Serial.println("DEMO RELOADED ram snapshot");
  }else if(parsed.action==Action::Advance){
    if(!demo_.advance(parsed.seconds,millis())){Serial.println("DEMO ERR time range");return true;}
    product_.tick(time());Serial.println("DEMO ADVANCED");
  }
  status();return true;
}
void ProductRuntime::command(const char* line) {
  if(!std::strcmp(line,"ble start")){
    if(!kBleDevelopment||demo_.active()||ble_.active())Serial.println("BLE ERR unavailable");
    else {setupRequested_=true;Serial.println("BLE setup requested");}return;
  }
  if(!std::strcmp(line,"ble stop")){setupRequested_=false;ble_.stop(clock_);resetPresentation();Serial.println("BLE stopped");return;}
  if(!std::strcmp(line,"ble status")){status();return;}
  if(ble_.active()&&std::strcmp(line,"status")){Serial.println("ERR BLE setup active; ble stop first");return;}
  if(demoCommand(line))return;
  // Whitelist before any storage/network/configuration dispatcher. Test writes
  // cannot escape via USB imports, backups, settings, image uploads or pairing.
  if(demo_.active()&&!diagnostics::demoAllowsCommand(line)){Serial.println("DEMO ERR command unavailable; demo stop first");return;}
  const bool imagesReady=assets_.ready();
  if(assets_.command(line)){if(assets_.active()){motion_.skip();inputSession_.invalidate();board_.inputContext(0);}dirty_=dirty_||(imagesReady!=assets_.ready());return;}
  if(assets_.active()){Serial.println("ERR asset upload active");return;}
  if(network_.command(line))return;
  if(storageCommand(line,store_,product_.state()))return;
  if(!std::strcmp(line,"motion on")||!std::strcmp(line,"motion off")){
    const bool enabled=!std::strcmp(line,"motion on");
    if(!demo_.active()){Preferences settings;
      if(!settings.begin("routine-ui",false)||settings.putBool("motion",enabled)!=1){Serial.println("ERR motion setting");return;}}
    motion_.enable(enabled);dirty_=true;Serial.println(enabled?"OK motion on":"OK motion off");return;
  }
  using application::TransferResult;
  const auto transferred=transfer_.receive(line,millis());
  if (transferred!=TransferResult::Other) {
    if(transferred==TransferResult::Complete){
      const auto decoded=application::decodeState(transfer_.bytes(),transfer_.size(),importScratch_);
      if(decoded!=application::DecodeResult::Ok || importScratch_.generation || importScratch_.eventCount){Serial.println("LOAD ERR payload");return;}
      schedule_={};schedule_.revision=importScratch_.scheduleRevision;schedule_.count=importScratch_.count;schedule_.routines=importScratch_.routines;
      const auto result=product_.importSchedule(schedule_);
      Serial.printf("LOAD RESULT %u\n",unsigned(result));
    }else if(transferred==TransferResult::Invalid)Serial.println("LOAD ERR transfer");
    else Serial.printf("LOAD OK %u\n",unsigned(transfer_.received()));
    return;
  }
  if(!std::strcmp(line,"help")){
    Serial.println("status | sync | idle off | idle 1..3600 | sleep 1..86400 | reboot");
    Serial.println("input previous | input next | input confirm; tools/import-schedule.ps1; tools/configure-wifi.ps1");
    Serial.println("api status | api sync | api reset-uuid; tools/configure-api.ps1; battery: fixed at 100%");
    Serial.println("motion on | motion off | storage info | storage list; tools/backup-state.ps1 [-All]");
    Serial.println("demo start | demo stop | demo status | demo advance 1..86400 | demo reload (RAM only)");return;
  }
  if(!std::strcmp(line,"reboot")){Serial.println("OK reboot");Serial.flush();ESP.restart();return;}
  if(!std::strncmp(line,"input ",6)){
    if(!assets_.ready()){Serial.println("ERR assets missing");return;}
    const char* name=line+6;
    const auto intent=!std::strcmp(name,"previous")?input::Intent::Previous:!std::strcmp(name,"next")?input::Intent::Next:!std::strcmp(name,"confirm")?input::Intent::Confirm:input::Intent::None;
    if(intent==input::Intent::None){Serial.println("ERR input");return;}
    dispatchInput(intent);status();return;
  }
  auto parsed=diagnostics::parseDeviceCommand(line);
  using Kind=diagnostics::DeviceCommandKind;
  switch(parsed.kind){
    case Kind::Status:status();break;
    case Kind::Sync:clock_.requestSync();Serial.println("OK sync");break;
    case Kind::WifiSetup:Serial.println(clock_.configure(parsed.ssid,parsed.password)?"OK network test started; saved after IP":"ERR network");break;
    case Kind::Idle:idleMs_=parsed.seconds*1000;Serial.println("OK idle");break;
    case Kind::Sleep:pendingSleep_=parsed.seconds;break;
    default:Serial.println("ERR command");break;
  }
  parsed.clearSecrets();
}
void ProductRuntime::dispatchInput(input::Intent intent){
  if(motion_.transient(millis())){motion_.skip();dirty_=true;return;}
  product_.input(intent,time());
}
void ProductRuntime::sleep(uint32_t seconds){
  if(demo_.active()||ble_.active()||setupRequested_||clock_.setupState()==provisioning::JoinState::Connecting||!seconds||board_.inputBusy()||!board_.canPresent()||transfer_.active()||assets_.active()||network_.busy()||!assets_.ready())return;
  Serial.printf("Sleep: %lus\n",static_cast<unsigned long>(seconds));Serial.flush();Serial.end();
  const auto result=sleepAndResync(board_,clock_,seconds);lastSleep_=result;
  beginUsbConsole();network_.request();activityAt_=millis();dirty_=true;
  Serial.printf("Wake: error=%d cause=%d requests=%lu\n",result.error,int(result.cause),static_cast<unsigned long>(clock_.requestCount()));
}
void ProductRuntime::tick(){
  if(!ready_){delay(10);return;}
  board_.updateBuzzer();clock_.tick();
  if(board_.takeSetupRequest()&&!demo_.active()){
    if(ble_.active()){ble_.stop(clock_);resetPresentation();}else setupRequested_=kBleDevelopment;
  }
  if(!demo_.active())network_.tick(product_,clock_,kBatteryPercent,!ble_.active()&&!setupRequested_&&clock_.setupState()!=provisioning::JoinState::Connecting&&!assets_.active()&&!transfer_.active());
  if(setupRequested_&&!network_.busy()&&!assets_.active()&&!transfer_.active()&&!demo_.active()&&clock_.setupState()!=provisioning::JoinState::Connecting){
    setupRequested_=false;inputSession_.invalidate();board_.inputContext(0);board_.armButton();motion_.skip();
    if(!ble_.start())Serial.println("BLE ERR start");shownSetupStatus_=nullptr;dirty_=true;
  }
  const bool wasSetup=ble_.active();ble_.tick(clock_,network_);
  if(wasSetup&&!ble_.active())resetPresentation();
  if(!inputSession_.matches(product_.state(),product_.view())){inputSession_.invalidate();board_.inputContext(0);}
  const auto event=board_.pollInput();const auto intent=event.intent;
  if(board_.inputBusy()||intent!=input::Intent::None)activityAt_=millis();
  // Controller input itself advances time and rejects presses for changed screens.
  if(!assets_.active()&&!ble_.active()){
    if(intent!=input::Intent::None&&assets_.ready()&&inputSession_.accepts(event.context,product_.state(),product_.view())){
      // A delayed skip press from the last animation frame is still only a skip.
      if(inputSession_.transient()){motion_.skip();dirty_=true;product_.tick(time());}
      else dispatchInput(intent);
    }else {if(intent!=input::Intent::None)++discardedInputs_;product_.tick(time());}
  }else if(ble_.active())product_.tick(time());
  assets_.tick();
  for(unsigned i=0;i<64&&Serial.available()&&!assets_.receiving();++i){
    const auto result=lines_.push(char(Serial.read()));
    if(result==diagnostics::LineResult::Ready){activityAt_=millis();command(lines_.line());lines_.clear();}
    else if(result==diagnostics::LineResult::TooLong){Serial.println("ERR line");lines_.clear();}
  }
  transfer_.tick(millis());
  if(product_.takeAlert())board_.beep(80);
  if(ble_.active()){
    if((dirty_||shownSetupStatus_!=ble_.status())&&board_.canPresent()){
      std::fill_n(board_.pixels(),240*320,uint16_t(0xffff));
      ui::drawing::text(board_.pixels(),"Wi-Fi setup",120,55,22,0);
      ui::drawing::text(board_.pixels(),ble_.name(),120,100,18,0);
      ui::drawing::text(board_.pixels(),ble_.status(),120,150,18,0);
      ui::drawing::text(board_.pixels(),"Open the app",120,200,16,0);
      ui::drawing::text(board_.pixels(),"D3 hold 5s: close",120,245,16,0);
      ui::drawing::text(board_.pixels(),"DEVELOPMENT",120,290,16,0xf800);
      board_.present();shownSetupStatus_=ble_.status();dirty_=false;
    }
    delay(1);return;
  }
  uint16_t minutes=0;bool known=false;
  if(demo_.active()){known=true;minutes=uint16_t(((time().unixSeconds+9*3600)/60)%1440);}
  else known=clock_.localMinutes(minutes);
  motion_.observe(product_.state(),product_.view(),millis());const auto motion=motion_.sample(millis());
  dirty_=dirty_||motion.kind!=shownMotion_||motion.frame!=shownFrame_;
  dirty_=dirty_||revision_!=product_.revision()||minutes_!=minutes||clockShown_!=known||wifiShown_!=clock_.connected();
  // Aborting/timing out a reinstall can keep the old pack ready. Re-publish
  // its input context even when no pixels or product state otherwise changed.
  dirty_=dirty_||(!assets_.active()&&assets_.ready()&&!inputSession_.context());
  if(dirty_&&board_.canPresent()&&!assets_.active()){
    const bool imagesReady=assets_.ready();
    const bool transient=ui::isTransient(motion.kind);
    if(!imagesReady||!inputSession_.matches(product_.state(),product_.view())||inputSession_.transient()!=transient){inputSession_.invalidate();board_.inputContext(0);}
    if(imagesReady)ui::renderProduct(board_.pixels(),product_.state(),product_.view(),known,minutes,motion);
    else {
      std::fill_n(board_.pixels(),240*320,uint16_t(0xffff));
      ui::drawing::text(board_.pixels(),"화면 자료를 확인해 주세요",120,148,18,0);
      ui::drawing::text(board_.pixels(),"USB 연결 후 설치",120,185,16,0);
    }
    if(assets_.ready())ui::renderDeviceStatus(board_.pixels(),clock_.connected(),kBatteryPercent);
    if(demo_.active()){
      ui::drawing::rect(board_.pixels(),78,0,84,20,0xffe0);
      ui::drawing::text(board_.pixels(),"TEST",120,16,16,0);
    }
    board_.present();dirty_=false;wifiShown_=clock_.connected();shownMotion_=motion.kind;shownFrame_=motion.frame;
    if(imagesReady&&!assets_.ready())dirty_=true;
    if(assets_.ready()){inputSession_.publish(product_.state(),product_.view(),transient);board_.inputContext(inputSession_.context());}
    minutes_=minutes;clockShown_=known;revision_=product_.revision();
  }
  if(pendingSleep_){const auto seconds=pendingSleep_;pendingSleep_=0;sleep(product_.sleepSeconds(time(),seconds));}
  else if(idleMs_&&uint32_t(millis()-activityAt_)>=idleMs_&&!clock_.sync().busy()&&!Serial.available())sleep(product_.sleepSeconds(time()));
  delay(1);
}
}
