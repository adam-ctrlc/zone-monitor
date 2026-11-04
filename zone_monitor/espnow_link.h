#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// The same 32-byte layout is used by the camera and servo boards.
#define LINK_TEXT_SIZE 32

typedef struct {
  char text[LINK_TEXT_SIZE];
} LinkMessage;

// Returns false if ESP-NOW failed to start
bool espNowSetup(void);

// Takes the oldest message from the camera. Returns false if none
// is waiting. out->text always ends in '\0'.
bool espNowReceive(LinkMessage* out);

void sendToServo(const char* text);

#ifdef __cplusplus
}
#endif
