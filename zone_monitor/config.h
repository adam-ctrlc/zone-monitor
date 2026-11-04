#pragma once

// All pins and tuning values live here.
// This file is included from both C and C++ code.

#include <stdint.h>

// Wi-Fi, website address, device key and MAC addresses. Git ignores this
// file. If it is missing, copy secrets.example.h to secrets.h and fill it in.
#include "secrets.h"

// ---------------- Wi-Fi ----------------
static const uint8_t ESPNOW_CHANNEL = 1;

// ---------------- TFT ----------------
#define TFT_CS   37
#define TFT_DC   38
#define TFT_MOSI 35
#define TFT_SCLK 21
#define TFT_RST  39

// Screen speed. If the picture is garbled, use 20000000.
// With short wires you can try 40000000.
#define TFT_SPI_HZ 27000000

// ---------------- BTS7960 ----------------
#define RPWM 14
#define LPWM 15
#define R_EN 16
#define L_EN 17

static const uint8_t SPEED = 200;
static const uint32_t RAMP_TIME = 500;

static const uint32_t PWM_FREQ = 20000;

// ---------------- LED STRIP ----------------
#define NUM_LEDS 56
#define LED_PIN 41
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 100

static const uint32_t REFRESH_MS = 500;

// ---------------- BUZZER ----------------
#define BUZZER_PIN 2

static const uint32_t BUZZ_MS = 500;

// ---------------- ZONES ----------------
static const uint8_t MAX_LEVEL = 3;

// ---------------- QR PAUSE ----------------
// Longest the belt waits for a visible QR to be read
static const uint32_t QR_PAUSE_TIMEOUT_MS = 3000;

// After giving up, no new pause for this long, so the
// unreadable QR can move out of the camera view
static const uint32_t QR_PAUSE_COOLDOWN_MS = 2000;

// ---------------- WEBSITE ----------------
// How often the website is asked for E-stop and reset commands
static const uint32_t NET_POLL_MS = 500;

static const int NET_TIMEOUT_MS = 2000;

// ---------------- DIAGNOSTICS ----------------
// Prints the slowest loop() time this often. 0 turns it off.
static const uint32_t LOOP_STATS_MS = 10000;
