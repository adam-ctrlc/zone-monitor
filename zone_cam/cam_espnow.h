#pragma once

void espNowSetup();

// Sends a scanned destination to the S3 and the servo board.
// Does not wait, so scanning continues straight away.
void sendDestination(const char* destination);

// Asks the S3 to pause the belt: a QR is visible but unreadable
void sendQrSeen();
