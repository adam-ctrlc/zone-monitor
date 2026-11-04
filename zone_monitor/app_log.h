#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Prints one line to Serial, printf style. Long lines are cut
// short rather than overflowing. Call from loop() or setup() only.
void appLog(const char* format, ...) __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif
