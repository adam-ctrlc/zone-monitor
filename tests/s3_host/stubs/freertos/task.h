#pragma once
#include "freertos/FreeRTOS.h"
BaseType_t xTaskCreatePinnedToCore(void (*task)(void*), const char* name, uint32_t stack, void* arg, int priority, void* handle, int core);
