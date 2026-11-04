#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Starts the TFT and draws the first full screen
void displaySetup(void);

void drawServerStatus(bool online);

// Redraws only the parts of a card that show the level
void drawCardLevel(int index);

void drawEstopBanner(void);
void clearEstopBanner(void);

#ifdef __cplusplus
}
#endif
