#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZONE_COUNT 6

typedef struct {
  const char* name;
  uint8_t level;
} Zone;

extern Zone zones[ZONE_COUNT];

void updateZone(int index, uint8_t newLevel);

// Applies zone resets requested by the website
void handleZoneResets(void);

// Acts on a scanned zone name from the camera
void zoneOnScan(const char* text);

#ifdef __cplusplus
}
#endif
