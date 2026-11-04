// The Arduino WiFi library is C++, so this file stays C++.

#include <WiFi.h>
#include <esp_wifi.h>

#include "config.h"
#include "wifi_link.h"

static unsigned long lastWifiTry = 0;

void wifiSetup() {

  WiFi.mode(WIFI_STA);

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

  if (
    WiFi.status() == WL_CONNECTED
  ) {

    Serial.print("IP address: ");

    Serial.println(
      WiFi.localIP()
    );

    Serial.print("Wi-Fi channel: ");

    Serial.println(
      WiFi.channel()
    );

  } else {

    Serial.println(
      "Wi-Fi failed - fallback ESP-NOW channel"
    );

    esp_wifi_set_promiscuous(true);

    esp_wifi_set_channel(
      ESPNOW_CHANNEL,
      WIFI_SECOND_CHAN_NONE
    );

    esp_wifi_set_promiscuous(false);
  }
}

void wifiReconnectUpdate() {

  if (
    WiFi.status() != WL_CONNECTED &&
    millis() - lastWifiTry > 10000
  ) {

    lastWifiTry = millis();

    WiFi.reconnect();
  }
}

bool wifiIsConnected() {

  return WiFi.status() == WL_CONNECTED;
}
