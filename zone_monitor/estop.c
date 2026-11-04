#include <stdatomic.h>

#include "app_log.h"
#include "display.h"
#include "espnow_link.h"
#include "estop.h"
#include "lighting.h"
#include "motor.h"

// Written by the website task, read by loop().
static atomic_uint wanted = 0;

// Set for every stop request. Without it, a stop followed quickly
// by a resume could leave motorHold() set with nothing to release it.
static atomic_uint stopPending = 0;

// Used by loop() only.
static bool active = false;

static void applyEstop(bool on) {
  active = on;

  if (on) {
    motorHold(MOTOR_HOLD_ESTOP);

    lightingSetEstop(true);

    drawEstopBanner();

    sendToServo("ESTOP");

    appLog("EMERGENCY STOP");

  } else {
    motorRelease(MOTOR_HOLD_ESTOP);

    lightingSetEstop(false);

    clearEstopBanner();

    sendToServo("RESUME");

    appLog("System resumed");
  }
}

void estopRequest(bool on) {
  if (on) {
    motorHold(MOTOR_HOLD_ESTOP);

    atomic_store(&wanted, 1u);

    atomic_store(&stopPending, 1u);

  } else {
    atomic_store(&wanted, 0u);
  }
}

bool estopActive(void) {
  return active;
}

void estopUpdate(void) {
  bool stopSeen = atomic_exchange(&stopPending, 0u) != 0;

  bool on = stopSeen || atomic_load(&wanted) != 0;

  if (on != active) {
    applyEstop(on);
  }
}
