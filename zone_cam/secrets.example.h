#pragma once

// Copy this file to secrets.h and fill it in. Git ignores secrets.h.

#define WIFI_SSID "your-wifi-name"
#define WIFI_PASS "your-wifi-password"

// The computer running the website, as seen from the boards' Wi-Fi network.
#define SERVER_URL "http://192.168.1.100:3000"

// Must match DEVICE_KEY in web/.env and zone_monitor/secrets.h.
#define DEVICE_KEY "change-me"

// Read a board's MAC address with:
//   esptool --port <port> read-mac

// ESP32-S3 main controller
const uint8_t receiverMAC1[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Standard ESP32 servo controller
const uint8_t receiverMAC2[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
