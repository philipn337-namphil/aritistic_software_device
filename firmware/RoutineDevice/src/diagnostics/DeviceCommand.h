#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\diagnostics\\DeviceCommand.h"
#pragma once
#include <cstdint>
#include <cstring>

namespace routine::diagnostics {
enum class DeviceCommandKind { Other, Invalid, Status, Sync, AutoClock, Sleep, Idle, WifiSetup };
struct DeviceCommand {
  DeviceCommandKind kind = DeviceCommandKind::Other;
  uint32_t seconds = 0;
  char ssid[33]{};
  char password[64]{};
  void clearSecrets() {
    volatile char* first = ssid;
    volatile char* second = password;
    for (size_t i = 0; i < sizeof(ssid); ++i) first[i] = 0;
    for (size_t i = 0; i < sizeof(password); ++i) second[i] = 0;
  }
};
inline bool secondsValue(const char* value, uint32_t max, uint32_t& out) {
  out = 0;
  if (!*value) return false;
  for (; *value; ++value) {
    if (*value < '0' || *value > '9') return false;
    const unsigned digit = *value - '0';
    if (out > (max - digit) / 10) return false;
    out = out * 10 + digit;
  }
  return out <= max;
}
inline int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
inline bool decodeHex(const char* start, size_t length, char* output, size_t capacity) {
  if (length % 2 || length / 2 >= capacity) return false;
  for (size_t i = 0; i < length; i += 2) {
    const int hi = hexDigit(start[i]), lo = hexDigit(start[i + 1]);
    if (hi < 0 || lo < 0 || !(hi || lo)) return false;
    output[i / 2] = static_cast<char>((hi << 4) | lo);
  }
  output[length / 2] = 0;
  return true;
}
inline DeviceCommand parseDeviceCommand(const char* line) {
  DeviceCommand command;
  if (!line) { command.kind = DeviceCommandKind::Invalid; return command; }
  if (!std::strcmp(line, "status")) command.kind = DeviceCommandKind::Status;
  else if (!std::strcmp(line, "sync")) command.kind = DeviceCommandKind::Sync;
  else if (!std::strcmp(line, "time auto")) command.kind = DeviceCommandKind::AutoClock;
  else if (!std::strcmp(line, "idle off")) command.kind = DeviceCommandKind::Idle;
  else if (!std::strncmp(line, "sleep ", 6)) {
    command.kind = secondsValue(line + 6, 86400, command.seconds) && command.seconds
                       ? DeviceCommandKind::Sleep : DeviceCommandKind::Invalid;
  } else if (!std::strncmp(line, "idle ", 5)) {
    command.kind = secondsValue(line + 5, 3600, command.seconds) && command.seconds
                       ? DeviceCommandKind::Idle : DeviceCommandKind::Invalid;
  } else if (!std::strncmp(line, "wifi-set ", 9)) {
    command.kind = DeviceCommandKind::Invalid;
    const char* split = std::strchr(line + 9, ' ');
    if (split && decodeHex(line + 9, static_cast<size_t>(split - line - 9), command.ssid, sizeof(command.ssid)) &&
        *command.ssid && decodeHex(split + 1, std::strlen(split + 1), command.password, sizeof(command.password))) {
      const size_t count = std::strlen(command.password);
      bool valid = count == 0 || (count >= 8 && count <= 63);
      for (size_t i = 0; i < count; ++i)
        if (static_cast<unsigned char>(command.password[i]) < 32 || static_cast<unsigned char>(command.password[i]) > 126) valid = false;
      if (valid) command.kind = DeviceCommandKind::WifiSetup;
    }
    if (command.kind == DeviceCommandKind::Invalid) command.clearSecrets();
  }
  return command;
}
}
