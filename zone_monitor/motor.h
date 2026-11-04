#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Reasons the belt is held. It runs only when none are active.
#define MOTOR_HOLD_ESTOP 1u
#define MOTOR_HOLD_QR    2u

void motorSetup(void);

// Applies the soft start. Call from loop().
void motorUpdate(void);

// Restarts the soft start. Call from loop() or setup() only.
void motorRestartRamp(void);

// Stops the motor at once. Safe to call from any task.
void motorHold(uint32_t reason);

// Removes a hold. When no holds remain, the belt soft-starts.
// Call from loop() only.
void motorRelease(uint32_t reason);

#ifdef __cplusplus
}
#endif
