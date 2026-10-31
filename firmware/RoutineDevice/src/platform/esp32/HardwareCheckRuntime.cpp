#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\HardwareCheckRuntime.cpp"
#include "HardwareCheckRuntime.h"
#include "PowerSession.h"
#include "UsbConsole.h"
#include "../../diagnostics/DeviceCommand.h"
#include <cstring>
#include <FFat.h>
#include <algorithm>
#include "../../presentation/Drawing.h"

namespace routine::platform {
void HardwareCheckRuntime::help() {
  Serial.println("RoutineDevice: HARDWARE CHECK v0.3 (sample growth/icons; live clock)");
  Serial.println("Buttons: D4=Previous/80ms beep; D5=Confirm/40ms beep; D6=Next/theme");
  Serial.println("Commands (newline): theme pink | theme dark | progress 0..100 | time HH:MM | time off | beep | help");
  Serial.println("Device: status | sync | time auto | sleep 1..86400 | idle 1..3600 | idle off");
  Serial.println("Wi-Fi initial setup: tools/configure-wifi.ps1 (credentials are never echoed)");
}
void HardwareCheckRuntime::begin() {
  beginUsbConsole();
  ready_ = board_.begin();
  if (!ready_) { Serial.println("FATAL: framebuffer allocation failed"); return; }
  // Diagnostic mode never formats storage or creates a product ledger.
  assets_.begin(FFat.begin(false));
  clock_.begin();
  updateClockView();
  if(assets_.ready())ui::renderHome(board_.pixels(), displayView_);
  else {
    std::fill_n(board_.pixels(),240*320,uint16_t(0xffff));
    ui::drawing::text(board_.pixels(),"화면 자료를 확인해 주세요",120,160,18,0);
  }
  board_.present();
  dirty_ = false;
  board_.armButton();
  activityAt_ = millis();
  help();
  Serial.printf("Framebuffer: %u bytes; PSRAM total: %u bytes\n",
                static_cast<unsigned>(ui::kWidth * ui::kHeight * sizeof(uint16_t)),
                static_cast<unsigned>(ESP.getPsramSize()));
}
void HardwareCheckRuntime::apply(diagnostics::Effects effects) {
  dirty_ = dirty_ || effects.redraw;
  if (effects.buzzerMs) board_.beep(effects.buzzerMs);
  switch (effects.reply) {
    case diagnostics::Reply::Ok: Serial.println("OK (preview state; not saved)"); break;
    case diagnostics::Reply::InvalidCommand: Serial.println("ERR: invalid command; send help"); break;
    case diagnostics::Reply::Help: help(); break;
    case diagnostics::Reply::ThemeSwitched: Serial.println("Button: theme switched"); break;
    case diagnostics::Reply::ConfirmTest: Serial.println("Intent: Confirm (diagnostic pulse only)"); break;
    case diagnostics::Reply::BuzzerTest: Serial.println("Button: buzzer pulse"); break;
    case diagnostics::Reply::None: break;
  }
}
void HardwareCheckRuntime::status() {
  const char* state = "unknown";
  switch (clock_.sync().state()) {
    case application::SyncState::NeedsSetup: state = "needs-setup"; break;
    case application::SyncState::Connecting: state = "connecting"; break;
    case application::SyncState::AwaitingTime: state = "awaiting-time"; break;
    case application::SyncState::Current: state = "current"; break;
    case application::SyncState::RetryWait: state = "retry-wait"; break;
    case application::SyncState::Suspended: state = "suspended"; break;
  }
  Serial.printf("Clock: %s; configured=%d connected=%d known=%d fresh=%d requests=%lu successes=%lu\n",
                state, clock_.configured(), clock_.connected(), clock_.sync().known(), clock_.sync().fresh(),
                static_cast<unsigned long>(clock_.requestCount()), static_cast<unsigned long>(clock_.successCount()));
  Serial.printf("Power: idle=%lus last-error=%d wake-cause=%d; clock-view=%s\n",
                static_cast<unsigned long>(idleMs_ / 1000), lastSleep_.error, int(lastSleep_.cause),
                clockOverride_ ? "sample" : "system");
}
void HardwareCheckRuntime::command(const char* line) {
  const bool imagesReady=assets_.ready();
  if(assets_.command(line)){dirty_=dirty_||(imagesReady!=assets_.ready());return;}
  if(assets_.active()){Serial.println("ERR asset upload active");return;}
  auto parsed = diagnostics::parseDeviceCommand(line);
  using Kind = diagnostics::DeviceCommandKind;
  switch (parsed.kind) {
    case Kind::Status: status(); break;
    case Kind::Sync: clock_.requestSync(); Serial.println("OK: time synchronization requested"); break;
    case Kind::AutoClock: clockOverride_ = false; dirty_ = true; Serial.println("OK: live clock view"); break;
    case Kind::Sleep: pendingSleep_ = parsed.seconds; break;
    case Kind::Idle: idleMs_ = parsed.seconds * 1000; Serial.println("OK: diagnostic idle interval set (RAM only)"); break;
    case Kind::WifiSetup:
      Serial.println(clock_.configure(parsed.ssid, parsed.password)
                       ? "OK: network saved; connection and time sync requested"
                       : "ERR: network settings rejected");
      break;
    case Kind::Invalid: Serial.println("ERR: invalid device command"); break;
    case Kind::Other: {
      const auto effects = check_.onCommand(line);
      if (effects.reply == diagnostics::Reply::Ok && !std::strncmp(line, "time ", 5)) clockOverride_ = true;
      apply(effects);
      break;
    }
  }
  parsed.clearSecrets();
}
void HardwareCheckRuntime::updateClockView() {
  auto next = check_.view();
  if (!clockOverride_) {
    next.minutesOfDay = 0;
    next.clockSet = clock_.localMinutes(next.minutesOfDay);
  }
  if (displayView_.clockSet != next.clockSet || displayView_.minutesOfDay != next.minutesOfDay) dirty_ = true;
  displayView_ = next;
}
void HardwareCheckRuntime::sleep(uint32_t seconds) {
  if(assets_.active()||!assets_.ready())return;
  Serial.printf("Sleep: %lus maximum; D4/D5/D6 wake; time sync follows every return\n", static_cast<unsigned long>(seconds));
  Serial.flush();
  // S3 hardware CDC cannot respond during manual light sleep. An explicit
  // detach/attach lets the host enumerate it again without resetting product RAM.
  Serial.end();
  lastSleep_ = sleepAndResync(board_, clock_, seconds);
  beginUsbConsole();
  activityAt_ = millis();
  dirty_ = true;
  status();
}
void HardwareCheckRuntime::tick() {
  if (!ready_) { delay(10); return; }
  board_.updateBuzzer();
  const auto intent = board_.pollIntent();
  if (board_.inputBusy() || intent != input::Intent::None) activityAt_ = millis();
  apply(check_.onIntent(intent));
  assets_.tick();
  for (unsigned n = 0; n < 64 && Serial.available() && !assets_.receiving(); ++n) {
    const auto result = lines_.push(static_cast<char>(Serial.read()));
    if (result == diagnostics::LineResult::Ready) {
      activityAt_ = millis();
      command(lines_.line());
      lines_.clear();
      if (pendingSleep_) break;
    } else if (result == diagnostics::LineResult::TooLong) {
      Serial.println("ERR: command too long or invalid byte");
      lines_.clear();
    }
  }
  clock_.tick();
  if (lastSync_ != clock_.sync().state()) { status(); lastSync_ = clock_.sync().state(); }
  updateClockView();
  board_.updateBuzzer();
  // Synchronous LCD transfers must not extend an active buzzer pulse.
  if (dirty_ && board_.canPresent()) {
    if(assets_.ready())ui::renderHome(board_.pixels(), displayView_);
    board_.present();
    dirty_ = false;
  }
  if (pendingSleep_) {
    const auto seconds = pendingSleep_;
    pendingSleep_ = 0;
    sleep(seconds);
  } else if (idleMs_ && uint32_t(millis() - activityAt_) >= idleMs_ &&
             !board_.inputBusy() && board_.canPresent() && !clock_.sync().busy() && !Serial.available()) {
    sleep(60);  // Diagnostic timer; product scheduler supplies its own boundary.
  }
  delay(1);
}
}
