#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Called by the website task. Asking for a stop halts the motor
// at once; lights, screen and servo follow in estopUpdate().
void estopRequest(bool on);

// Current emergency stop state. Call from loop() only.
bool estopActive(void);

// Applies a requested change. Call from loop().
void estopUpdate(void);

#ifdef __cplusplus
}
#endif
