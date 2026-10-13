#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\diagnostics\\LineBuffer.h"
#pragma once
#include <cstddef>

namespace routine::diagnostics {
enum class LineResult { None, Ready, TooLong };
template <size_t Capacity>
class BasicLineBuffer {
 public:
  static constexpr size_t kCapacity = Capacity;
  static_assert(Capacity >= 2, "Line buffer needs data and terminator");
  LineResult push(char c) {
    if (c == '\r') return LineResult::None;
    if (c == '\n') {
      const auto result = rejected_ ? LineResult::TooLong : length_ ? LineResult::Ready : LineResult::None;
      data_[length_] = '\0';
      length_ = 0;
      rejected_ = false;
      return result;
    }
    if (!c || length_ + 1 >= kCapacity) rejected_ = true;
    if (!rejected_) data_[length_++] = c;
    return LineResult::None;
  }
  const char* line() const { return data_; }
  void clear() {
    volatile char* bytes = data_;
    for (size_t i = 0; i < kCapacity; ++i) bytes[i] = 0;
    length_ = 0;
    rejected_ = false;
  }
 private:
  char data_[kCapacity]{};
  size_t length_ = 0;
  bool rejected_ = false;
};
using LineBuffer = BasicLineBuffer<64>;
}
