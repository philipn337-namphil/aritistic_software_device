#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\HardwareConfig.h"
#pragma once
#include <Arduino.h>

namespace routine::hardware {
// XIAO ESP32-S3 Plus board labels from the user's actual wiring.
constexpr uint8_t kSclk = D8;
constexpr uint8_t kMosi = D10;
constexpr uint8_t kCs = D2;
constexpr uint8_t kReset = D1;
constexpr uint8_t kDc = D0;
constexpr uint8_t kButton = D3;
constexpr uint8_t kBuzzer = D11;
static_assert(D8 == 7 && D10 == 9 && D2 == 3 && D1 == 2 && D0 == 1 &&
              D3 == 4 && D11 == 38, "Select XIAO_ESP32S3_Plus board definition");

// Initial assumption: button closes to GND; NPN buzzer driver is active HIGH.
constexpr bool kButtonActiveLow = true;
constexpr bool kBuzzerActiveHigh = true;
constexpr uint8_t kRotation = 0;  // 0 or 2 keep the 240 x 320 portrait layout.
constexpr bool kInvertDisplay = true;  // ST7789 library default; verify on panel.
constexpr uint32_t kSpiFrequency = 20000000;
static_assert(kRotation == 0 || kRotation == 2, "This UI requires portrait mode");
// BL and the user-labelled SC pin are unused. No GPIO is assigned to them.
}
