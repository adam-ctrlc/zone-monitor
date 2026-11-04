#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Handles every waiting message from the camera, in order:
// QR_SEEN pauses the belt; QR_CLEARED or a zone name beeps and
// resumes it; a zone name also updates that zone.
void handleCameraMessages(void);

#ifdef __cplusplus
}
#endif
