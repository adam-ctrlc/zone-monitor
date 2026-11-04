#pragma once

// All pins and tuning values live here.

#include <Arduino.h>

// Wi-Fi, website address, device key and MAC addresses. Git ignores this
// file. If it is missing, copy secrets.example.h to secrets.h and fill it in.
#include "secrets.h"

// =====================================================
// AI THINKER ESP32-CAM PIN CONFIGURATION
// =====================================================

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5

#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_PIN          4

// =====================================================
// CAMERA SETTINGS
// =====================================================

// Keep CIF because your QR code is only 3 cm.
// CIF = 400 x 296 pixels.

#define FRAME_W       400
#define FRAME_H       296
#define FRAME_SIZE    FRAMESIZE_CIF

// Native grayscale avoids the YUV-to-grayscale
// conversion previously performed on every frame.

#define PIX_FORMAT    PIXFORMAT_GRAYSCALE
#define BYTES_PER_PIXEL 1

// Fixed exposure keeps the QR sharp on a moving belt.
// Lower CAM_EXPOSURE = less blur but a darker picture.
// Higher CAM_GAIN = brighter but noisier picture.
// These are starting values. Try others from the website
// without reflashing, then copy the best ones here.

#define CAM_EXPOSURE  120   // 0 to 1200
#define CAM_GAIN      8     // 0 to 30

// Part of the frame searched for QR codes.
// A smaller area is decoded faster. If parcels always pass
// through the same strip, shrink this to fit that strip.

#define SCAN_X        0
#define SCAN_Y        0
#define SCAN_W        FRAME_W
#define SCAN_H        FRAME_H

// Used when a QR shape is found but cannot be read.
// The area around it is sharpened and decoded once more.

const int SHARPEN_AMOUNT = 3;    // Higher = stronger sharpening
const int SHARPEN_MARGIN = 16;   // Pixels kept around the shape

// =====================================================
// SCAN CONTROL
// =====================================================

// The same QR can be sent again once it has been
// out of view for this long.
const unsigned long CLEAR_MS = 2000;

// While a QR shape is visible but unreadable, the S3 is
// asked to pause the belt this often.
const unsigned long QR_SEEN_INTERVAL_MS = 200;

// Preview frames are only copied while the page is open.
const unsigned long PREVIEW_ACTIVE_MS = 2000;

// =====================================================
// WEBSITE (runs in the web folder, on a computer)
// =====================================================

const int UPLINK_TIMEOUT_MS = 1500;

// How often scanner status is sent to the website.
const unsigned long UPLINK_STATUS_MS = 1000;
