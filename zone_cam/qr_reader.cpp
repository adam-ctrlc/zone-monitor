#include "esp_camera.h"
#include "esp_memory_utils.h"
#include "esp_timer.h"
#include "quirc.h"

#include "cam_config.h"
#include "cam_espnow.h"
#include "cam_sensor.h"
#include "qr_reader.h"

// Arduino compiles for size by default. Applies to this file's own code.
#pragma GCC optimize("O2")

static struct quirc* q = NULL;

static struct quirc_code code;
static struct quirc_data data;

static quirc_decode_error_t err;

// Used only when normal decoding fails.
static struct quirc_code flipped;

// Used by the QR task only.
static char lastText[32] = "";

static unsigned long lastSeen = 0;

static unsigned long lastQrSeenSent = 0;

// Read by the website task on the other core, so it is only
// touched while holding statusLock. Keep those sections short.
static portMUX_TYPE statusLock = portMUX_INITIALIZER_UNLOCKED;
static QrStatus status = {};

// =====================================================
// MIRRORED QR SUPPORT
// =====================================================

static void flipCode(struct quirc_code* c) {

  memset(
    &flipped,
    0,
    sizeof(flipped)
  );

  flipped.size = c->size;

  for (int y = 0; y < c->size; y++) {

    for (int x = 0; x < c->size; x++) {

      int from = y * c->size + x;

      if (
        c->cell_bitmap[from >> 3] &
        (1 << (from & 7))
      ) {

        int to = x * c->size + y;

        flipped.cell_bitmap[to >> 3] |=
          (1 << (to & 7));
      }
    }
  }

  *c = flipped;
}

// =====================================================
// QR DECODING
// =====================================================

// Normal decoding is attempted first.
// Mirrored decoding is only attempted if necessary.
// lastErr receives the most recent failure, or 0.

static bool decodeAny(int* lastErr) {

  int count = quirc_count(q);

  *lastErr = 0;

  for (int i = 0; i < count; i++) {

    quirc_extract(
      q,
      i,
      &code
    );

    // First attempt: normal QR.
    err = quirc_decode(
      &code,
      &data
    );

    if (err == QUIRC_SUCCESS) {
      return true;
    }

    // Second attempt: mirrored QR.
    flipCode(&code);

    err = quirc_decode(
      &code,
      &data
    );

    if (err == QUIRC_SUCCESS) {
      return true;
    }

    *lastErr = (int)err;
  }

  return false;
}

// =====================================================
// BLURRED QR RETRY
// =====================================================

// Only runs after a QR shape was found but could not be
// read, so normal scanning is not slowed down.
// Noise is reduced first because sharpening a noisy picture
// directly makes it harder to read, not easier.

static uint8_t* sharpenA = NULL;
static uint8_t* sharpenB = NULL;
static uint8_t* sharpenC = NULL;

// 3x3 smoothing of one area. The output uses SCAN_W rows.
static void smoothArea(
  const uint8_t* src,
  int srcStride,
  uint8_t* dst,
  int x0,
  int y0,
  int x1,
  int y1
) {

  for (int y = y0; y < y1; y++) {

    const uint8_t* a = src + (y - 1) * srcStride;
    const uint8_t* b = src + y * srcStride;
    const uint8_t* c = src + (y + 1) * srcStride;

    uint8_t* out = dst + y * SCAN_W;

    for (int x = x0; x < x1; x++) {

      out[x] = (
        a[x - 1] + 2 * a[x] + a[x + 1] +
        2 * b[x - 1] + 4 * b[x] + 2 * b[x + 1] +
        c[x - 1] + 2 * c[x] + c[x + 1]
      ) >> 4;
    }
  }
}

// Finds the box around every QR shape in the last decode.
static bool shapeArea(int* x0, int* y0, int* x1, int* y1) {

  int count = quirc_count(q);

  int minX = SCAN_W;
  int minY = SCAN_H;
  int maxX = 0;
  int maxY = 0;

  for (int i = 0; i < count; i++) {

    quirc_extract(q, i, &code);

    for (int c = 0; c < 4; c++) {

      if (code.corners[c].x < minX) minX = code.corners[c].x;
      if (code.corners[c].x > maxX) maxX = code.corners[c].x;
      if (code.corners[c].y < minY) minY = code.corners[c].y;
      if (code.corners[c].y > maxY) maxY = code.corners[c].y;
    }
  }

  // Stay 3 pixels inside the scan area: the smoothing
  // passes read up to 3 pixels beyond this box.
  *x0 = max(minX - SHARPEN_MARGIN, 3);
  *y0 = max(minY - SHARPEN_MARGIN, 3);
  *x1 = min(maxX + SHARPEN_MARGIN, SCAN_W - 3);
  *y1 = min(maxY + SHARPEN_MARGIN, SCAN_H - 3);

  return count > 0 && *x1 > *x0 && *y1 > *y0;
}

// "frame" points at the scan area of the camera picture.
static bool decodeSharpened(const uint8_t* frame) {

  int x0, y0, x1, y1;

  if (
    sharpenA == NULL ||
    !shapeArea(&x0, &y0, &x1, &y1)
  ) {

    return false;
  }

  // Lightly smoothed picture.
  smoothArea(frame, FRAME_W, sharpenA, x0 - 2, y0 - 2, x1 + 2, y1 + 2);

  // More heavily smoothed picture.
  smoothArea(sharpenA, SCAN_W, sharpenB, x0 - 1, y0 - 1, x1 + 1, y1 + 1);
  smoothArea(sharpenB, SCAN_W, sharpenC, x0, y0, x1, y1);

  uint8_t* image = quirc_begin(
    q,
    NULL,
    NULL
  );

  memset(image, 128, SCAN_W * SCAN_H);

  // Boost the difference between the two pictures.
  for (int y = y0; y < y1; y++) {

    for (int x = x0; x < x1; x++) {

      int light = sharpenA[y * SCAN_W + x];

      int value =
        light +
        SHARPEN_AMOUNT * (light - sharpenC[y * SCAN_W + x]);

      image[y * SCAN_W + x] =
        value < 0 ? 0 : (value > 255 ? 255 : value);
    }
  }

  quirc_end(q);

  int ignored;

  return decodeAny(&ignored);
}

// =====================================================
// DECODED TEXT
// =====================================================

// Copies the QR text without surrounding spaces. A longer text is cut
// to fit the 32-byte ESP-NOW message. out always ends in '\0'.
static void cleanText(
  const struct quirc_data* source,
  char* out,
  size_t outSize
) {

  size_t length = source->payload_len > 0
    ? strnlen(
        (const char*)source->payload,
        (size_t)source->payload_len
      )
    : 0;

  size_t start = 0;

  while (start < length && isspace(source->payload[start])) {
    start++;
  }

  while (length > start && isspace(source->payload[length - 1])) {
    length--;
  }

  size_t count = min(length - start, outSize - 1);

  memcpy(out, source->payload + start, count);

  out[count] = '\0';
}

// =====================================================
// QR READER TASK
// =====================================================

static void QRCodeReader(void* pvParameters) {

  uint32_t frameCount = 0;

  int fps = 0;

  unsigned long fpsStart = millis();

  bool fastMemory = esp_ptr_internal(
    quirc_begin(q, NULL, NULL)
  );

  Serial.println(
    fastMemory
      ? "QR decoder memory: internal (fast)"
      : "QR decoder memory: PSRAM (a smaller scan area may fit internal)"
  );

  while (true) {

    // Get camera frame.
    camera_fb_t* fb = esp_camera_fb_get();

    if (fb == NULL) {

      vTaskDelay(1);

      continue;
    }

    // Confirm correct frame size.
    if (
      fb->len !=
      (size_t)(FRAME_W * FRAME_H * BYTES_PER_PIXEL)
    ) {

      esp_camera_fb_return(fb);

      vTaskDelay(1);

      continue;
    }

    int64_t frameStartUs = esp_timer_get_time();

    // -------------------------------------------------
    // UPDATE WEBSITE PREVIEW
    // -------------------------------------------------

    previewUpdate(fb);

    // -------------------------------------------------
    // DIRECT GRAYSCALE INPUT TO QUIRC
    // -------------------------------------------------

    uint8_t* image = quirc_begin(
      q,
      NULL,
      NULL
    );

    // Copy only the scan area.
    for (int y = 0; y < SCAN_H; y++) {

      memcpy(
        image + y * SCAN_W,
        fb->buf + (SCAN_Y + y) * FRAME_W + SCAN_X,
        SCAN_W
      );
    }

    quirc_end(q);

    // -------------------------------------------------
    // DECODE QR
    // -------------------------------------------------

    int decodeErr = 0;

    bool decoded = decodeAny(&decodeErr);

    // Shape found but unreadable: sharpen it and try again.
    // Keep the first result for the website and belt pause.
    int shapes = quirc_count(q);

    if (!decoded && shapes > 0) {

      decoded = decodeSharpened(
        fb->buf + SCAN_Y * FRAME_W + SCAN_X
      );

      if (decoded) {

        decodeErr = 0;
      }
    }

    // The retry needs the original picture, so the camera
    // buffer is only returned here.
    esp_camera_fb_return(fb);

    char text[sizeof(lastText)] = "";

    if (decoded) {

      cleanText(&data, text, sizeof(text));

      lastSeen = millis();

      // Send only when the QR changes or
      // the previous QR has left the camera view.
      if (
        text[0] != '\0' &&
        strcmp(text, lastText) != 0
      ) {

        strcpy(lastText, text);

        sendDestination(text);
      }
    }

    // -------------------------------------------------
    // QR VISIBLE BUT UNREADABLE: ASK THE BELT TO PAUSE
    // -------------------------------------------------

    if (
      !decoded &&
      shapes > 0 &&
      lastText[0] == '\0' &&
      millis() - lastQrSeenSent >= QR_SEEN_INTERVAL_MS
    ) {

      lastQrSeenSent = millis();

      sendQrSeen();
    }

    // -------------------------------------------------
    // RESET SCAN AFTER QR LEAVES THE CAMERA VIEW
    // -------------------------------------------------

    if (
      !decoded &&
      lastText[0] != '\0' &&
      millis() - lastSeen > CLEAR_MS
    ) {

      lastText[0] = '\0';

      Serial.println(
        "Ready for next parcel"
      );
    }

    // -------------------------------------------------
    // STATUS FOR THE WEBSITE
    // -------------------------------------------------

    frameCount++;

    if (millis() - fpsStart >= 1000) {

      fps = frameCount;

      frameCount = 0;

      fpsStart = millis();
    }

    int frameMs = (int)((esp_timer_get_time() - frameStartUs) / 1000);

    unsigned long now = millis();

    portENTER_CRITICAL(&statusLock);

    status.fps = fps;
    status.frameMs = frameMs;
    status.shapes = shapes;
    status.decodeErr = decodeErr;
    status.fastMemory = fastMemory;

    if (decoded) {

      memcpy(status.lastText, text, sizeof(status.lastText));

      status.decodedAt = now;
    }

    portEXIT_CRITICAL(&statusLock);

    vTaskDelay(1);
  }
}

void qrReaderGetStatus(QrStatus* out) {

  portENTER_CRITICAL(&statusLock);

  *out = status;

  portEXIT_CRITICAL(&statusLock);
}

void qrReaderSetup() {

  if (!camOk) {

    return;
  }

  q = quirc_new();

  if (
    q == NULL ||
    quirc_resize(
      q,
      SCAN_W,
      SCAN_H
    ) < 0
  ) {

    Serial.println(
      "Quirc initialization failed"
    );

    camOk = false;

  } else {

    Serial.println(
      "QR decoder ready"
    );

    sharpenA = (uint8_t*)ps_malloc(SCAN_W * SCAN_H);
    sharpenB = (uint8_t*)ps_malloc(SCAN_W * SCAN_H);
    sharpenC = (uint8_t*)ps_malloc(SCAN_W * SCAN_H);

    if (
      sharpenA == NULL ||
      sharpenB == NULL ||
      sharpenC == NULL
    ) {

      // Decoding still works, just without the retry.
      sharpenA = NULL;

      Serial.println(
        "Not enough PSRAM - blurred QR retry is off"
      );
    }
  }
}

void qrReaderStart() {

  if (camOk) {

    // Core 1 is left to QR scanning. Wi-Fi, the camera driver
    // and the website upload all run on core 0.
    xTaskCreatePinnedToCore(
      QRCodeReader,
      "QRCodeReader_Task",
      20000,
      NULL,
      2,
      NULL,
      1
    );

    Serial.println(
      "QR reader task started"
    );
  }
}
