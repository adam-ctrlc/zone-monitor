#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void lightingSetup(void);
void lightingUpdate(void);

// true = red (emergency stop), false = white (normal)
void lightingSetEstop(bool on);

#ifdef __cplusplus
}
#endif
