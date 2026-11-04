#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// One bit per zone. This value means all six.
#define NET_RESET_ALL_MASK 0x3Fu

// Creates the zone update queue and starts the website task
void networkStart(void);

// Queues a zone level to be sent to the website
void networkSendLevel(int zone, uint8_t level);

// Queues a scanned QR text to be sent to the website
void networkSendScan(const char* text);

// Returns the zones the website asked to reset, then clears them
uint32_t networkTakeResets(void);

// Updates the screen when the website connection changes
void serverStatusUpdate(void);

#ifdef __cplusplus
}
#endif
