#pragma once

#include <Arduino.h>
#include "esp_camera.h"

extern bool camOk;

void flashSetup();
void flashSet(bool on);

// Starts the camera and applies the image settings
void cameraSetup();

// Copies a frame for the website while the page is open
void previewUpdate(camera_fb_t* fb);

// Keeps preview frames coming for the next couple of seconds
void previewRequest();

// Encodes the latest preview. The caller frees *jpg.
// Fails if no recent frame is available.
bool previewToJpg(uint8_t** jpg, size_t* jpgLen);

// Changes exposure and gain until the next restart
bool cameraTune(bool hasAec, int aec, bool hasGain, int gain);
