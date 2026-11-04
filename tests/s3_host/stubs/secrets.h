#pragma once

// Fake settings, so the tests run without a private secrets.h.

#define WIFI_SSID "test"
#define WIFI_PASS "test"
#define SERVER_URL "http://test:3000"
#define DEVICE_KEY "test"

static const uint8_t CAM_MAC[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
static const uint8_t SERVO_MAC[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02};
