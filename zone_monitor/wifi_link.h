#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void wifiSetup(void);

// Retries the Wi-Fi connection when it has dropped
void wifiReconnectUpdate(void);

// Safe to call from any task
bool wifiIsConnected(void);

#ifdef __cplusplus
}
#endif
