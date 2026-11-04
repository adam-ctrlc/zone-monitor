#include <stdbool.h>
#include <string.h>

#include "app_log.h"
#include "buzzer.h"
#include "clock_ms.h"
#include "config.h"
#include "espnow_link.h"
#include "estop.h"
#include "motor.h"
#include "qr_clear.h"
#include "zones.h"

// A read arrives as a zone name and then QR_CLEARED. Beep once.
#define READ_DEDUPE_MS 300

// Upper limit per loop() pass, so messages cannot stall the rest.
#define MAX_MESSAGES_PER_PASS 16

static uint32_t lastReadTime = 0;

static bool pausedForQr = false;
static uint32_t pauseStart = 0;
static uint32_t lastGiveUp = 0;

static void onQrSeen(void) {
  uint32_t now = clockMs();

  if (
    estopActive() ||
    pausedForQr ||
    now - lastGiveUp < QR_PAUSE_COOLDOWN_MS
  ) {
    return;
  }

  pausedForQr = true;
  pauseStart = now;

  motorHold(MOTOR_HOLD_QR);

  appLog("QR visible but unread - belt paused");
}

static void onQrRead(void) {
  // A read during an emergency stop is ignored.
  if (estopActive()) {
    return;
  }

  uint32_t now = clockMs();

  bool duplicate = now - lastReadTime < READ_DEDUPE_MS;

  lastReadTime = now;

  if (!duplicate) {
    buzzerBeep();

    appLog("QR read");
  }

  if (pausedForQr) {
    pausedForQr = false;

    motorRelease(MOTOR_HOLD_QR);

    appLog("Belt resumed");
  }
}

static void handlePauseTimeout(void) {
  uint32_t now = clockMs();

  if (pausedForQr && now - pauseStart >= QR_PAUSE_TIMEOUT_MS) {
    pausedForQr = false;
    lastGiveUp = now;

    motorRelease(MOTOR_HOLD_QR);

    appLog("QR not read in time - belt resumed");
  }
}

void handleCameraMessages(void) {
  LinkMessage message;

  for (int i = 0; i < MAX_MESSAGES_PER_PASS && espNowReceive(&message); i++) {
    if (strcmp(message.text, "QR_SEEN") == 0) {
      onQrSeen();

    } else if (strcmp(message.text, "QR_CLEARED") == 0) {
      onQrRead();

    } else if (message.text[0] != '\0') {
      // Resume the belt first; drawing the screen comes after.
      onQrRead();

      zoneOnScan(message.text);
    }
  }

  handlePauseTimeout();
}
