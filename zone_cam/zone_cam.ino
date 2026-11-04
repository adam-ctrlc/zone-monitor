#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_log.h"

#include "cam_espnow.h"
#include "cam_sensor.h"
#include "cam_uplink.h"
#include "cam_wifi.h"
#include "qr_reader.h"

void setup() {

  WRITE_PERI_REG(
    RTC_CNTL_BROWN_OUT_REG,
    0
  );

  Serial.begin(115200);

  esp_log_level_set(
    "cam_hal",
    ESP_LOG_NONE
  );

  flashSetup();

  wifiSetup();

  cameraSetup();

  qrReaderSetup();

  espNowSetup();

  qrReaderStart();

  uplinkStart();

  // Flash ON after initialization.
  flashSet(true);

  Serial.println(
    "ESP32-CAM READY"
  );
}

void loop() {

  // Scanning and uploads run in their own tasks.
  wifiReconnectUpdate();

  delay(100);
}
