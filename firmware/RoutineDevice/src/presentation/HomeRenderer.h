#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\presentation\\HomeRenderer.h"
#pragma once
#include <cstdint>

namespace routine::ui {
enum class Theme : uint8_t { Pink, Dark, Horned, GreenRobot, Cat, Dog, Seal, BlueRobot, Bear, Rabbit };
struct HomeViewModel {
  Theme theme = Theme::Pink;
  uint8_t progress = 0;
  bool clockSet = false;
  uint16_t minutesOfDay = 0;
  bool showProgress = true;
  uint8_t stage = 0; // 0 retains the original diagnostic reference; product uses 1..3.
};
constexpr int kWidth = 240;
constexpr int kHeight = 320;
void renderHome(uint16_t* pixels, const HomeViewModel& state);
}
