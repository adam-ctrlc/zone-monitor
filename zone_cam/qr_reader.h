#pragma once

#include <Arduino.h>

// A consistent copy of the scanner's state for the website
struct QrStatus {
  int fps;
  int frameMs;          // Time for the most recent frame
  int shapes;
  int decodeErr;
  bool fastMemory;      // Decoder is using internal RAM
  char lastText[32];    // Always ends in '\0'
  unsigned long decodedAt;
};

// Safe to call from any task
void qrReaderGetStatus(QrStatus* out);

// Sets up the decoder. Clears camOk if that fails.
void qrReaderSetup();

// Starts the scanning task
void qrReaderStart();
