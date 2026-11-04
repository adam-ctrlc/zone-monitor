#include <WiFi.h>

#include "cam_config.h"
#include "cam_wifi.h"

static unsigned long lastWifiTry = 0;

void wifiSetup() {

  WiFi.mode(WIFI_STA);

  WiFi.persistent(false);

  WiFi.disconnect(true);

  delay(200);

  WiFi.setSleep(false);

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASS
  );

  Serial.print("Connecting to Wi-Fi");

  unsigned long t0 = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - t0 < 15000
  ) {

    delay(500);

    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("Wi-Fi connected");

    Serial.print("IP address: ");

    Serial.println(WiFi.localIP());

    Serial.print("Wi-Fi channel: ");

    Serial.println(WiFi.channel());

  } else {

    Serial.println(
      "Wi-Fi connection failed"
    );

    Serial.println(
      "QR scanning can still operate"
    );
  }
}

void wifiReconnectUpdate() {

  if (
    WiFi.status() != WL_CONNECTED &&
    millis() - lastWifiTry > 10000
  ) {

    lastWifiTry = millis();

    Serial.println(
      "Reconnecting to Wi-Fi..."
    );

    WiFi.disconnect();

    WiFi.begin(
      WIFI_SSID,
      WIFI_PASS
    );
  }
}
