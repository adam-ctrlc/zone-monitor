#include "app_log.h"
#include "buzzer.h"
#include "display.h"
#include "espnow_link.h"
#include "estop.h"
#include "config.h"
#include "lighting.h"
#include "motor.h"
#include "net_link.h"
#include "qr_clear.h"
#include "wifi_link.h"
#include "zones.h"

static uint32_t slowestLoopUs = 0;
static uint32_t lastLoopStats = 0;

// Shows the slowest loop() recently, to check real speed on the board.
static void loopStatsUpdate(uint32_t loopStartUs) {

  if (LOOP_STATS_MS == 0) {

    return;
  }

  uint32_t loopUs = micros() - loopStartUs;

  if (loopUs > slowestLoopUs) {

    slowestLoopUs = loopUs;
  }

  if (millis() - lastLoopStats >= LOOP_STATS_MS) {

    lastLoopStats = millis();

    appLog("Slowest loop: %lu us", (unsigned long)slowestLoopUs);

    slowestLoopUs = 0;
  }
}

void setup() {

  Serial.begin(115200);

  motorSetup();

  lightingSetup();

  buzzerSetup();

  displaySetup();

  wifiSetup();

  bool linkOk = espNowSetup();

  // Started even if ESP-NOW failed, so the website E-stop still works.
  networkStart();

  // Wi-Fi can take 15 seconds. Without this the belt would skip
  // its soft start and jump straight to full speed.
  motorRestartRamp();

  Serial.println(
    linkOk ? "System ready" : "System ready WITHOUT camera link"
  );
}

void loop() {

  uint32_t loopStartUs = micros();

  estopUpdate();

  serverStatusUpdate();

  motorUpdate();

  lightingUpdate();

  buzzerUpdate();

  wifiReconnectUpdate();

  handleCameraMessages();

  handleZoneResets();

  loopStatsUpdate(loopStartUs);
}
