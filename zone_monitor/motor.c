#include <stdatomic.h>
#include <stdbool.h>

#include "driver/gpio.h"
#include "driver/ledc.h"

#include "app_log.h"
#include "clock_ms.h"
#include "config.h"
#include "motor.h"

#define MOTOR_LEDC_MODE    LEDC_LOW_SPEED_MODE
#define MOTOR_LEDC_TIMER   LEDC_TIMER_0
#define MOTOR_LEDC_FORWARD LEDC_CHANNEL_0
#define MOTOR_LEDC_REVERSE LEDC_CHANNEL_1

// Written by any task, so it must be atomic.
static atomic_uint holdMask = 0;

// Used by loop() and setup() only.
static uint32_t rampStart = 0;
static uint32_t lastDuty = 0;

static void pwmWrite(ledc_channel_t channel, uint32_t duty) {
  ledc_set_duty(MOTOR_LEDC_MODE, channel, duty);
  ledc_update_duty(MOTOR_LEDC_MODE, channel);
}

static void enablePin(gpio_num_t pin) {
  gpio_reset_pin(pin);
  gpio_set_direction(pin, GPIO_MODE_OUTPUT);
  gpio_set_level(pin, 1);
}

static bool channelSetup(ledc_channel_t channel, int pin) {
  ledc_channel_config_t config = {
    .gpio_num = pin,
    .speed_mode = MOTOR_LEDC_MODE,
    .channel = channel,
    .intr_type = LEDC_INTR_DISABLE,
    .timer_sel = MOTOR_LEDC_TIMER,
    .duty = 0,
    .hpoint = 0,
  };

  return ledc_channel_config(&config) == ESP_OK;
}

void motorSetup(void) {
  ledc_timer_config_t timer = {
    .speed_mode = MOTOR_LEDC_MODE,
    .duty_resolution = LEDC_TIMER_8_BIT,
    .timer_num = MOTOR_LEDC_TIMER,
    .freq_hz = PWM_FREQ,
    .clk_cfg = LEDC_AUTO_CLK,
  };

  bool ok = ledc_timer_config(&timer) == ESP_OK;

  // Duty starts at 0, so enable the driver after the PWM is ready.
  ok = channelSetup(MOTOR_LEDC_FORWARD, RPWM) && ok;
  ok = channelSetup(MOTOR_LEDC_REVERSE, LPWM) && ok;

  enablePin((gpio_num_t)R_EN);
  enablePin((gpio_num_t)L_EN);

  rampStart = clockMs();

  appLog(ok ? "Motor initialized" : "Motor PWM setup FAILED");
}

void motorRestartRamp(void) {
  rampStart = clockMs();
}

void motorHold(uint32_t reason) {
  atomic_fetch_or(&holdMask, reason);

  // Do not wait for loop(). lastDuty is left alone on purpose: if
  // loop() was mid-write, its next pass sees the hold and writes 0.
  pwmWrite(MOTOR_LEDC_FORWARD, 0);
}

void motorRelease(uint32_t reason) {
  uint32_t before = atomic_fetch_and(&holdMask, ~reason);

  if ((before & reason) != 0 && (before & ~reason) == 0) {
    motorRestartRamp();
  }
}

void motorUpdate(void) {
  uint32_t duty = 0;

  if (atomic_load(&holdMask) == 0) {
    uint32_t elapsed = clockMs() - rampStart;

    // elapsed is below RAMP_TIME here, so this cannot overflow
    // for any sensible RAMP_TIME, and it never divides by zero.
    duty = elapsed < RAMP_TIME ? elapsed * SPEED / RAMP_TIME : SPEED;
  }

  // Only touch the hardware when the value changes.
  if (duty != lastDuty) {
    pwmWrite(MOTOR_LEDC_FORWARD, duty);
    lastDuty = duty;
  }
}
