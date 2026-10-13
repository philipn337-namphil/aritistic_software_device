#line 1 "C:\\Users\\phili\\Downloads\\Yeso2026-master\\Yeso2026-master\\firmware\\RoutineDevice\\src\\platform\\esp32\\PowerSession.h"
#pragma once
#include "Board.h"
#include "NetworkClock.h"

namespace routine::platform {
// The shared sleep entry point. A product application first commits its pending
// state and supplies a duration bounded by its next schedule deadline.
inline SleepResult sleepAndResync(Board& board, NetworkClock& clock, uint32_t seconds) {
  if (board.inputBusy() || !board.canPresent() || !seconds || seconds > 86400)
    return {ESP_ERR_INVALID_STATE, ESP_SLEEP_WAKEUP_UNDEFINED};
  const bool stopped = clock.suspend();
  const auto result = stopped ? board.lightSleep(seconds)
                              : SleepResult{ESP_FAIL, ESP_SLEEP_WAKEUP_UNDEFINED};
  // Even a rejected hardware sleep must restore connectivity after suspension.
  // All successful wakes (button or timer) unconditionally request NEW NTP data.
  clock.requestSync();
  return result;
}
}
