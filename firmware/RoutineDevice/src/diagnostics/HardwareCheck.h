#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\diagnostics\\HardwareCheck.h"
#pragma once
#include "../presentation/HomeRenderer.h"
#include "../input/Intent.h"

namespace routine::diagnostics {
enum class Reply { None, Ok, InvalidCommand, Help, ThemeSwitched, ConfirmTest, BuzzerTest };
struct Effects {
  bool redraw = false;
  uint16_t buzzerMs = 0;
  Reply reply = Reply::None;
};
// Sample data belongs only to this hardware check, never to product balances.
class HardwareCheck {
 public:
  HardwareCheck();
  const ui::HomeViewModel& view() const { return view_; }
  Effects onIntent(input::Intent intent);
  Effects onCommand(const char* line);
 private:
  ui::HomeViewModel view_{};
};
}
