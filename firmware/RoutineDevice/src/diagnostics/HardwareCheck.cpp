#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\diagnostics\\HardwareCheck.cpp"
#include "HardwareCheck.h"
#include <cstring>

namespace routine::diagnostics {
namespace {
bool number(const char*& text, unsigned max, unsigned& value) {
  value = 0;
  if (*text < '0' || *text > '9') return false;
  while (*text >= '0' && *text <= '9') {
    const unsigned digit = static_cast<unsigned>(*text++ - '0');
    if (digit > max || value > (max - digit) / 10) return false;
    value = value * 10 + digit;
  }
  return true;
}
}
HardwareCheck::HardwareCheck() {
  view_.progress = 44;
  view_.clockSet = true;
  view_.minutesOfDay = 9 * 60 + 10;
}
Effects HardwareCheck::onIntent(input::Intent intent) {
  if (intent == input::Intent::Next) {
    view_.theme = view_.theme == ui::Theme::Pink ? ui::Theme::Dark : ui::Theme::Pink;
    return {true, 0, Reply::ThemeSwitched};
  }
  if (intent == input::Intent::Confirm) return {false, 40, Reply::ConfirmTest};
  if (intent == input::Intent::Previous) return {false, 80, Reply::BuzzerTest};
  return {};
}
Effects HardwareCheck::onCommand(const char* line) {
  if (!line) return {false, 0, Reply::InvalidCommand};
  if (std::strcmp(line, "help") == 0) return {false, 0, Reply::Help};
  if (std::strcmp(line, "beep") == 0) return {false, 80, Reply::Ok};
  if (std::strcmp(line, "theme pink") == 0) view_.theme = ui::Theme::Pink;
  else if (std::strcmp(line, "theme dark") == 0) view_.theme = ui::Theme::Dark;
  else if (std::strcmp(line, "time off") == 0) view_.clockSet = false;
  else if (std::strncmp(line, "progress ", 9) == 0) {
    const char* cursor = line + 9;
    unsigned progress;
    if (!number(cursor, 100, progress) || *cursor) return {false, 0, Reply::InvalidCommand};
    view_.progress = static_cast<uint8_t>(progress);
  } else if (std::strncmp(line, "time ", 5) == 0) {
    const char* cursor = line + 5;
    unsigned hour, minute;
    if (!number(cursor, 23, hour) || *cursor++ != ':' ||
        !number(cursor, 59, minute) || *cursor) return {false, 0, Reply::InvalidCommand};
    view_.minutesOfDay = static_cast<uint16_t>(hour * 60 + minute);
    view_.clockSet = true;
  } else return {false, 0, Reply::InvalidCommand};
  return {true, 0, Reply::Ok};
}
}
