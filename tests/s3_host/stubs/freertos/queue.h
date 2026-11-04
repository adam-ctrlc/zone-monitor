#pragma once
#include "freertos/FreeRTOS.h"
typedef struct fake_queue* QueueHandle_t;
QueueHandle_t xQueueCreate(uint32_t length, uint32_t itemSize);
BaseType_t xQueueSend(QueueHandle_t queue, const void* item, TickType_t wait);
BaseType_t xQueueReceive(QueueHandle_t queue, void* item, TickType_t wait);
