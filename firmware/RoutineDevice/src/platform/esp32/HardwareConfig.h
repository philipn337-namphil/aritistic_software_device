#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\HardwareConfig.h"
#pragma once
#include <Arduino.h>

namespace routine::hardware {
// XIAO ESP32-S3 Plus board labels from the user's actual wiring.
constexpr uint8_t kSclk = D9;
constexpr uint8_t kMosi = D10;
constexpr uint8_t kCs = D2;
constexpr uint8_t kReset = D1;
constexpr uint8_t kDc = D0;
// Order matches input::Control: Previous, Confirm, Next.
constexpr uint8_t kButtonCount = 3;
constexpr uint8_t kButtons[kButtonCount] = {D4, D5, D6};
constexpr uint8_t kBuzzer = D8;
static_assert(D9 == 8 && D10 == 9 && D2 == 3 && D1 == 2 && D0 == 1 &&
              D4 == 5 && D5 == 6 && D6 == 43 && D8 == 7,
              "Select XIAO_ESP32S3_Plus board definition");

// Initial assumption: button closes to GND; NPN buzzer driver is active HIGH.
constexpr bool kButtonActiveLow = true;
constexpr bool kBuzzerActiveHigh = true;
constexpr uint8_t kRotation = 0;  // 0 or 2 keep the 240 x 320 portrait layout.
constexpr bool kInvertDisplay = true;  // ST7789 library default; verify on panel.
constexpr uint32_t kSpiFrequency = 20000000;
static_assert(kRotation == 0 || kRotation == 2, "This UI requires portrait mode");
// BL and the user-labelled SC pin are unused. No GPIO is assigned to them.
}
