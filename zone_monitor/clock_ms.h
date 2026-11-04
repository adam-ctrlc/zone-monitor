#pragma once

#include <stdint.h>

#include "esp_timer.h"

// Milliseconds since boot. Wraps after about 49 days, so compare
// times by subtracting them: clockMs() - start >= duration.
static inline uint32_t clockMs(void) {
  return (uint32_t)(esp_timer_get_time() / 1000);
}
