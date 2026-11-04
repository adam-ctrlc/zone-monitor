#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "clock_ms.h"
#include "config.h"
#include "display.h"
#include "espnow_link.h"
#include "estop.h"
#include "net_link.h"
#include "wifi_link.h"
#include "zones.h"

#define EVENT_QUEUE_LENGTH 10

// Replies larger than this are treated as a failed poll, so a cut-off
// reply can never hide an E-stop command.
#define COMMAND_BODY_SIZE 2048

typedef enum {
  NET_EVENT_LEVEL,
  NET_EVENT_SCAN,
} NetEventKind;

typedef struct {
  NetEventKind kind;
  int zone;
  uint8_t level;
  char text[LINK_TEXT_SIZE];
} NetEvent;

static QueueHandle_t eventQueue = NULL;

// Shared between the website task and loop().
static atomic_uint resetMask = 0;
static atomic_uint serverOnline = 0;

// Used by loop() only.
static bool shownOnline = false;

// Used by the website task only.
static esp_http_client_handle_t pollClient = NULL;
static esp_http_client_handle_t postClient = NULL;

static char commandBody[COMMAND_BODY_SIZE];
static size_t commandLength = 0;
static bool commandTooLong = false;

static uint32_t lastPoll = 0;
static uint8_t pollFails = 0;
static bool levelsSent = false;

// Collects the reply. It can arrive in several pieces.
static esp_err_t onPollEvent(esp_http_client_event_t* event) {
  if (event->event_id != HTTP_EVENT_ON_DATA || event->data == NULL || event->data_len <= 0) {
    return ESP_OK;
  }

  size_t incoming = (size_t)event->data_len;
  size_t room = sizeof(commandBody) - 1 - commandLength;

  if (incoming > room) {
    commandTooLong = true;
    incoming = room;
  }

  memcpy(commandBody + commandLength, event->data, incoming);

  commandLength += incoming;
  commandBody[commandLength] = '\0';

  return ESP_OK;
}

static esp_http_client_handle_t newClient(
  const char* url,
  esp_http_client_method_t method,
  http_event_handle_cb handler
) {
  esp_http_client_config_t config;

  memset(&config, 0, sizeof(config));

  config.url = url;
  config.method = method;
  config.timeout_ms = NET_TIMEOUT_MS;
  config.event_handler = handler;

  esp_http_client_handle_t client = esp_http_client_init(&config);

  if (client != NULL) {
    esp_http_client_set_header(client, "x-device-key", DEVICE_KEY);
  }

  return client;
}

// Returns false if the escaped text does not fit.
static bool jsonEscape(char* out, size_t outSize, const char* text) {
  if (outSize == 0) {
    return false;
  }

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

  out[used] = '\0';

  return true;
}

// payload must stay valid until this returns.
static void postJson(const char* url, const char* payload, int length) {
  if (!wifiIsConnected()) {
    return;
  }

  if (postClient == NULL) {
    postClient = newClient(url, HTTP_METHOD_POST, NULL);

    if (postClient == NULL) {
      return;
    }

    esp_http_client_set_header(postClient, "Content-Type", "application/json");
  }

  esp_http_client_set_url(postClient, url);

  esp_http_client_set_post_field(postClient, payload, length);

  esp_http_client_perform(postClient);

  // Stop the client pointing at the caller's buffer.
  esp_http_client_set_post_field(postClient, NULL, 0);

  // Updates are rare and the server may drop an idle connection,
  // so each one uses a fresh connection.
  esp_http_client_close(postClient);
}

static void postLevel(int zone, uint8_t level) {
  if (zone < 0 || zone >= ZONE_COUNT) {
    return;
  }

  char payload[96];

  int length = snprintf(
    payload,
    sizeof(payload),
    "{\"zone\":\"%s\",\"level\":%u}",
    zones[zone].name,
    (unsigned)level
  );

  if (length > 0 && length < (int)sizeof(payload)) {
    postJson(SERVER_URL "/api/v1/device/level", payload, length);
  }
}

static void postScan(const char* text) {
  // Six characters per byte at worst.
  char escaped[LINK_TEXT_SIZE * 6];

  if (!jsonEscape(escaped, sizeof(escaped), text)) {
    return;
  }

  char payload[sizeof(escaped) + 16];

  int length = snprintf(payload, sizeof(payload), "{\"text\":\"%s\"}", escaped);

  if (length > 0 && length < (int)sizeof(payload)) {
    postJson(SERVER_URL "/api/v1/device/scan", payload, length);
  }
}

// Brings the website up to date after this board or the website restarts.
static void postAllLevels(void) {
  for (int i = 0; i < ZONE_COUNT; i++) {
    postLevel(i, zones[i].level);
  }
}

static void handleEvent(const NetEvent* event) {
  if (event->kind == NET_EVENT_LEVEL) {
    postLevel(event->zone, event->level);

  } else {
    postScan(event->text);
  }
}

static void applyCommands(const char* body) {
  uint32_t resets = 0;

  if (strstr(body, "RESET_ALL") != NULL) {
    resets = NET_RESET_ALL_MASK;

  } else {
    for (int i = 0; i < ZONE_COUNT; i++) {
      char key[16];

      snprintf(key, sizeof(key), "RESET:%d", i);

      if (strstr(body, key) != NULL) {
        resets |= 1u << i;
      }
    }
  }

  if (resets != 0) {
    atomic_fetch_or(&resetMask, resets);
  }

  if (strstr(body, "\"estop\":true") != NULL) {
    estopRequest(true);

  } else if (strstr(body, "\"estop\":false") != NULL) {
    estopRequest(false);
  }
}

static bool pollOnce(void) {
  commandLength = 0;
  commandTooLong = false;
  commandBody[0] = '\0';

  esp_err_t result = esp_http_client_perform(pollClient);

  if (result != ESP_OK || esp_http_client_get_status_code(pollClient) != 200) {
    esp_http_client_close(pollClient);
    return false;
  }

  return !commandTooLong;
}

static void pollCommands(void) {
  if (pollClient == NULL) {
    pollClient = newClient(SERVER_URL "/api/v1/device/commands", HTTP_METHOD_GET, onPollEvent);

    if (pollClient == NULL) {
      return;
    }
  }

  bool ok = pollOnce();

  // The kept-open connection may have been dropped by the server.
  // If the last poll worked, try once more on a new connection.
  if (!ok && pollFails == 0 && !commandTooLong) {
    ok = pollOnce();
  }

  if (ok) {
    applyCommands(commandBody);

    pollFails = 0;

    atomic_store(&serverOnline, 1u);

    if (!levelsSent || strstr(commandBody, "\"needLevels\":true") != NULL) {
      levelsSent = true;

      postAllLevels();
    }

  } else {
    if (pollFails < 255) {
      pollFails++;
    }

    if (pollFails >= 2) {
      atomic_store(&serverOnline, 0u);
    }
  }
}

// One pass of the website task. Waits for a zone update until the
// next poll is due, so an update goes out as soon as it is queued.
static void netStep(void) {
  uint32_t sincePoll = clockMs() - lastPoll;

  TickType_t wait =
    sincePoll >= NET_POLL_MS ? 0 : pdMS_TO_TICKS(NET_POLL_MS - sincePoll);

  NetEvent event;

  if (xQueueReceive(eventQueue, &event, wait) == pdTRUE) {
    handleEvent(&event);
  }

  if (clockMs() - lastPoll < NET_POLL_MS) {
    return;
  }

  lastPoll = clockMs();

  if (wifiIsConnected()) {
    pollCommands();

  } else {
    atomic_store(&serverOnline, 0u);

    pollFails = 1;
  }
}

static void netTask(void* unused) {
  (void)unused;

  for (;;) {
    netStep();
  }
}

void networkStart(void) {
  // Failed polls are expected while the server is off.
  esp_log_level_set("HTTP_CLIENT", ESP_LOG_NONE);
  esp_log_level_set("transport_base", ESP_LOG_NONE);
  esp_log_level_set("esp-tls", ESP_LOG_NONE);

  eventQueue = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(NetEvent));

  if (eventQueue == NULL) {
    return;
  }

  xTaskCreatePinnedToCore(netTask, "netTask", 8192, NULL, 1, NULL, 0);
}

void networkSendLevel(int zone, uint8_t level) {
  if (eventQueue == NULL || zone < 0 || zone >= ZONE_COUNT) {
    return;
  }

  NetEvent event;

  memset(&event, 0, sizeof(event));

  event.kind = NET_EVENT_LEVEL;
  event.zone = zone;
  event.level = level;

  xQueueSend(eventQueue, &event, 0);
}

void networkSendScan(const char* text) {
  if (eventQueue == NULL || text == NULL) {
    return;
  }

  NetEvent event;

  // Zero first, so the copy below is always terminated.
  memset(&event, 0, sizeof(event));

  event.kind = NET_EVENT_SCAN;

  strncpy(event.text, text, sizeof(event.text) - 1);

  xQueueSend(eventQueue, &event, 0);
}

uint32_t networkTakeResets(void) {
  return atomic_exchange(&resetMask, 0u);
}

void serverStatusUpdate(void) {
  bool online = atomic_load(&serverOnline) != 0;

  if (online != shownOnline) {
    shownOnline = online;

    drawServerStatus(online);
  }
}
