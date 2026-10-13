#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\UsbConsole.h"
#pragma once
#include <Arduino.h>
namespace routine::platform {
inline void beginUsbConsole() {
  // HWCDC's default 256-byte queue drops a 4KiB binary asset chunk while the
  // LCD is sending a frame. Recreate this queue after Serial.end/light sleep too.
  const auto capacity=Serial.setRxBufferSize(8192);
  Serial.begin(115200);
  if(capacity<8192)Serial.println("ERR USB receive buffer allocation");
}
}
