#pragma once

// Speed settings for the quirc QR decoder. This file is not
// part of quirc; each quirc .c file includes it on line 1.

// The ESP32 has a hardware FPU for float but not for double.
#define QUIRC_FLOAT_TYPE float
#define QUIRC_USE_TGMATH

// Arduino compiles for size (-Os) by default.
#pragma GCC optimize("O2")
