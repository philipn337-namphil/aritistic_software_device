// Browser build: the same SimRuntime, driven from JavaScript instead of the PC
// host's HTTP loop. The page calls sim_tick() every few milliseconds and reads
// the framebuffer straight out of WebAssembly memory.
#ifdef __EMSCRIPTEN__
#include "SimRuntime.h"
#include <emscripten/emscripten.h>
#include <memory>
#include <string>

namespace {
std::unique_ptr<routine::sim::SimRuntime> device;
std::string text;   // backing store for strings returned to JavaScript
}

extern "C" {
EMSCRIPTEN_KEEPALIVE int sim_begin(const char* dataDirectory, const char* assetPack) {
  // ProductController/SnapshotStore hold several large snapshots: keep them off the stack.
  device = std::make_unique<routine::sim::SimRuntime>(dataDirectory, assetPack);
  return device->begin() ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE void sim_tick() { if (device) device->tick(); }
EMSCRIPTEN_KEEPALIVE void sim_buttons(int mask) { if (device) device->buttons(uint8_t(mask & 7)); }
EMSCRIPTEN_KEEPALIVE const char* sim_command(const char* line) {
  if (!device) return "";
  const size_t before = device->console().total();
  device->command(line);
  text = device->console().since(before);
  return text.c_str();
}
EMSCRIPTEN_KEEPALIVE const uint16_t* sim_pixels() { return device ? device->pixels() : nullptr; }
EMSCRIPTEN_KEEPALIVE unsigned sim_frames() { return device ? device->frames() : 0; }
EMSCRIPTEN_KEEPALIVE int sim_screen_on() { return device && device->screenOn() ? 1 : 0; }
EMSCRIPTEN_KEEPALIVE const char* sim_status() { text = device ? device->statusJson() : "{}"; return text.c_str(); }
EMSCRIPTEN_KEEPALIVE double sim_console_total() { return device ? double(device->console().total()) : 0; }
EMSCRIPTEN_KEEPALIVE const char* sim_console_since(double since) {
  text = device ? device->console().since(size_t(since)) : std::string();
  return text.c_str();
}
}
#endif
