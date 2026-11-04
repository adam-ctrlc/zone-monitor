#include <stdbool.h>

#include "driver/gpio.h"

#include "buzzer.h"
#include "clock_ms.h"
#include "config.h"

static bool buzzerOn = false;
static uint32_t buzzStart = 0;

void buzzerSetup(void) {
  gpio_reset_pin((gpio_num_t)BUZZER_PIN);
  gpio_set_direction((gpio_num_t)BUZZER_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)BUZZER_PIN, 0);

  buzzerOn = false;
}

void buzzerBeep(void) {
  if (buzzerOn) {
    return;
  }

  buzzerOn = true;
  buzzStart = clockMs();

  gpio_set_level((gpio_num_t)BUZZER_PIN, 1);
}

void buzzerUpdate(void) {
  if (buzzerOn && clockMs() - buzzStart >= BUZZ_MS) {
    buzzerOn = false;

    gpio_set_level((gpio_num_t)BUZZER_PIN, 0);
  }
}
