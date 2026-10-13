#if defined(ROUTINE_BLE_DEVELOPMENT) && ROUTINE_BLE_DEVELOPMENT
#include <BLEDevice.h>
#endif

#if defined(ROUTINE_HARDWARE_CHECK)
#include "src/platform/esp32/HardwareCheckRuntime.h"
routine::platform::HardwareCheckRuntime runtime;
#else
#include "src/platform/esp32/ProductRuntime.h"
// Large bounded snapshots live in external RAM, leaving internal RAM for USB,
// Wi-Fi/TLS and task stacks. Allocation is explicit and never silently shrinks.
#include <esp_heap_caps.h>
#include <new>
routine::platform::ProductRuntime* runtime=nullptr;
#endif

void setup() {
#if defined(ROUTINE_HARDWARE_CHECK)
  runtime.begin();
#else
  void* memory=heap_caps_malloc(sizeof(routine::platform::ProductRuntime),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!memory){Serial.begin(115200);Serial.println("FATAL product PSRAM allocation");return;}
  runtime=new(memory) routine::platform::ProductRuntime();runtime->begin();
#endif
}
void loop() {
#if defined(ROUTINE_HARDWARE_CHECK)
  runtime.tick();
#else
  if(runtime)runtime->tick();else delay(100);
#endif
}
