#include <stdio.h>
#include <strings.h>

#include "app_log.h"
#include "config.h"
#include "display.h"
#include "espnow_link.h"
#include "estop.h"
#include "net_link.h"
#include "zones.h"

Zone zones[ZONE_COUNT] = {
  {"Vamenta st.", 0},
  {"Lirio st.", 0},
  {"MaxSuniel st.", 0},
  {"P.N.Roa st.", 0},
  {"Gumamela st.", 0},
  {"Villarin st.", 0}
};

void updateZone(int index, uint8_t newLevel) {
  if (index < 0 || index >= ZONE_COUNT) {
    return;
  }

  if (zones[index].level == newLevel) {
    return;
  }

  zones[index].level = newLevel;

  drawCardLevel(index);
}

void handleZoneResets(void) {
  uint32_t mask = networkTakeResets();

  if (mask == 0) {
    return;
  }

  for (int i = 0; i < ZONE_COUNT; i++) {
    if (mask & (1u << i)) {
      updateZone(i, 0);
    }
  }

  if (mask == NET_RESET_ALL_MASK) {
    sendToServo("RESET_ALL");
    return;
  }

  for (int i = 0; i < ZONE_COUNT; i++) {
    if (mask & (1u << i)) {
      char command[LINK_TEXT_SIZE];

      snprintf(command, sizeof(command), "RESET:%d", i);

      sendToServo(command);
    }
  }
}

void zoneOnScan(const char* text) {
  // Ignore new scans during an emergency stop.
  if (text == NULL || estopActive()) {
    return;
  }

  appLog("Received: %s", text);

  networkSendScan(text);

  for (int i = 0; i < ZONE_COUNT; i++) {
    if (strcasecmp(text, zones[i].name) == 0) {
      if (zones[i].level < MAX_LEVEL) {
        updateZone(i, zones[i].level + 1);

        networkSendLevel(i, zones[i].level);
      }

      break;
    }
  }
}
