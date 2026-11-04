#pragma once
#include "esp_err.h"
#include <stdint.h>
typedef int gpio_num_t;
#define GPIO_MODE_OUTPUT 2
esp_err_t gpio_reset_pin(gpio_num_t pin);
esp_err_t gpio_set_direction(gpio_num_t pin, int mode);
esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level);
