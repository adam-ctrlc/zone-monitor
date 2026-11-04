#include <WiFi.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "quirc.h"

#include "cam_config.h"
#include "cam_sensor.h"
#include "cam_uplink.h"
#include "qr_reader.h"

// A larger reply is ignored rather than acted on when cut off.
static char replyBody[256];
static size_t replyLength = 0;
static bool replyTooLong = false;

static bool wantFrames = false;

static esp_err_t onStatusEvent(esp_http_client_event_t* event) {

  if (
    event->event_id != HTTP_EVENT_ON_DATA ||
    event->data == NULL ||
    event->data_len <= 0
  ) {

    return ESP_OK;
  }

  size_t incoming = (size_t)event->data_len;

  size_t room = sizeof(replyBody) - 1 - replyLength;

  if (incoming > room) {

    replyTooLong = true;

    incoming = room;
  }

  memcpy(replyBody + replyLength, event->data, incoming);

  replyLength += incoming;

  replyBody[replyLength] = '\0';

  return ESP_OK;
}

static esp_http_client_handle_t newClient(
  const char* url,
  const char* contentType,
  http_event_handle_cb handler
) {

  esp_http_client_config_t config = {};

  config.url = url;
  config.method = HTTP_METHOD_POST;
  config.timeout_ms = UPLINK_TIMEOUT_MS;
  config.event_handler = handler;

  esp_http_client_handle_t client = esp_http_client_init(&config);

  if (client != NULL) {

    esp_http_client_set_header(client, "x-device-key", DEVICE_KEY);
    esp_http_client_set_header(client, "Content-Type", contentType);
  }

  return client;
}

// Returns false if the escaped text does not fit.
static bool jsonEscape(char* out, size_t outSize, const char* text) {

  size_t used = 0;

  for (; *text != '\0'; text++) {

    unsigned char c = (unsigned char)*text;

    char piece[8];

    int length;

    if (c == '"' || c == '\\') {

      piece[0] = '\\';
      piece[1] = (char)c;

      length = 2;

    } else if (c < 0x20) {

      length = snprintf(piece, sizeof(piece), "\\u%04x", c);

    } else {

      piece[0] = (char)c;

      length = 1;
    }

    if (used + (size_t)length >= outSize) {

      return false;
    }

    memcpy(out + used, piece, (size_t)length);

    used += (size_t)length;
  }

  if (outSize == 0) {

    return false;
  }

  out[used] = '\0';

  return true;
}

// Reads a number following a key such as "aec":
static bool replyInt(const char* key, long* value) {

  const char* found = strstr(replyBody, key);

  if (found == NULL) {

    return false;
  }

  const char* start = found + strlen(key);

  char* end = NULL;

  long parsed = strtol(start, &end, 10);

  if (end == start) {

    return false;
  }

  *value = parsed;

  return true;
}

// Applies the website's camera settings, only when they change.
static void applyReply() {

  wantFrames = strstr(replyBody, "\"wantFrames\":true") != NULL;

  static int flashShown = -1;

  int flash = -1;

  if (strstr(replyBody, "\"flash\":true") != NULL) {

    flash = 1;

  } else if (strstr(replyBody, "\"flash\":false") != NULL) {

    flash = 0;
  }

  if (flash >= 0 && flash != flashShown) {

    flashShown = flash;

    flashSet(flash == 1);
  }

  static long aecShown = -1;
  static long gainShown = -1;

  long aec = -1;
  long gain = -1;

  bool hasAec = replyInt("\"aec\":", &aec) && aec != aecShown;
  bool hasGain = replyInt("\"gain\":", &gain) && gain != gainShown;

  if (hasAec || hasGain) {

    cameraTune(hasAec, (int)aec, hasGain, (int)gain);

    if (hasAec) aecShown = aec;
    if (hasGain) gainShown = gain;
  }
}

static void sendStatus() {

  static esp_http_client_handle_t client = NULL;

  if (client == NULL) {

    client = newClient(
      SERVER_URL "/api/v1/device/camera/status",
      "application/json",
      onStatusEvent
    );

    if (client == NULL) {

      return;
    }
  }

  QrStatus status;

  qrReaderGetStatus(&status);

  // Six characters per byte at worst.
  char escaped[sizeof(status.lastText) * 6];

  if (!jsonEscape(escaped, sizeof(escaped), status.lastText)) {

    escaped[0] = '\0';
  }

  long agoSeconds = status.decodedAt != 0
    ? (long)((millis() - status.decodedAt) / 1000)
    : -1;

  char payload[384];

  int length = snprintf(
    payload,
    sizeof(payload),
    "{\"ok\":%s,\"fps\":%d,\"frameMs\":%d,\"shapes\":%d,"
    "\"readError\":\"%s\",\"lastText\":\"%s\",\"lastAgoSeconds\":%ld,"
    "\"fastMemory\":%s}",
    camOk ? "true" : "false",
    status.fps,
    status.frameMs,
    status.shapes,
    status.decodeErr != 0
      ? quirc_strerror((quirc_decode_error_t)status.decodeErr)
      : "",
    escaped,
    agoSeconds,
    status.fastMemory ? "true" : "false"
  );

  if (length < 0 || length >= (int)sizeof(payload)) {

    return;
  }

  replyLength = 0;
  replyTooLong = false;
  replyBody[0] = '\0';

  esp_http_client_set_post_field(client, payload, length);

  esp_err_t result = esp_http_client_perform(client);

  if (
    result != ESP_OK ||
    esp_http_client_get_status_code(client) != 200
  ) {

    esp_http_client_close(client);

    wantFrames = false;

    return;
  }

  if (!replyTooLong) {

    applyReply();
  }
}

static void sendFrame() {

  static esp_http_client_handle_t client = NULL;

  if (client == NULL) {

    client = newClient(
      SERVER_URL "/api/v1/device/camera/frame",
      "image/jpeg",
      NULL
    );

    if (client == NULL) {

      return;
    }
  }

  uint8_t* jpg = NULL;

  size_t jpgLength = 0;

  if (!previewToJpg(&jpg, &jpgLength) || jpg == NULL) {

    return;
  }

  esp_http_client_set_post_field(
    client,
    (const char*)jpg,
    (int)jpgLength
  );

  esp_err_t result = esp_http_client_perform(client);

  // Stop the client pointing at the buffer freed below.
  esp_http_client_set_post_field(client, NULL, 0);

  free(jpg);

  if (
    result != ESP_OK ||
    esp_http_client_get_status_code(client) != 200
  ) {

    esp_http_client_close(client);
  }
}

static void uplinkTask(void* unused) {

  unsigned long lastStatus = 0;

  while (true) {

    if (WiFi.status() == WL_CONNECTED) {

      if (millis() - lastStatus >= UPLINK_STATUS_MS) {

        lastStatus = millis();

        sendStatus();
      }

      if (wantFrames) {

        previewRequest();

        sendFrame();
      }

    } else {

      wantFrames = false;
    }

    vTaskDelay(pdMS_TO_TICKS(wantFrames ? 100 : 200));
  }
}

void uplinkStart() {

  // Failed uploads are expected while the website is off.
  esp_log_level_set("HTTP_CLIENT", ESP_LOG_NONE);
  esp_log_level_set("transport_base", ESP_LOG_NONE);
  esp_log_level_set("esp-tls", ESP_LOG_NONE);

  xTaskCreatePinnedToCore(
    uplinkTask,
    "uplinkTask",
    8192,
    NULL,
    1,
    NULL,
    0
  );
}
