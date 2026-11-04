#include <string.h>

#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "app_log.h"
#include "config.h"
#include "espnow_link.h"

#define RX_QUEUE_LENGTH 16

// A queue keeps every message in order. The callback runs in the
// Wi-Fi task on the other core, so it must never wait.
static QueueHandle_t rxQueue = NULL;

static void onReceive(
  const esp_now_recv_info_t* info,
  const uint8_t* incoming,
  int len
) {
  if (rxQueue == NULL || info == NULL || incoming == NULL) {
    return;
  }

  if (memcmp(info->src_addr, CAM_MAC, sizeof(CAM_MAC)) != 0) {
    return;
  }

  if (len < (int)sizeof(LinkMessage)) {
    return;
  }

  LinkMessage message;

  memcpy(&message, incoming, sizeof(message));

  // The sender may not have ended the text properly.
  message.text[sizeof(message.text) - 1] = '\0';

  xQueueSend(rxQueue, &message, 0);
}

bool espNowSetup(void) {
  rxQueue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(LinkMessage));

  if (rxQueue == NULL) {
    appLog("ESP-NOW queue allocation failed");
    return false;
  }

  if (esp_now_init() != ESP_OK) {
    appLog("ESP-NOW init failed");
    return false;
  }

  if (esp_now_register_recv_cb(onReceive) != ESP_OK) {
    appLog("ESP-NOW receive setup failed");
    return false;
  }

  esp_now_peer_info_t peer;

  memset(&peer, 0, sizeof(peer));

  memcpy(peer.peer_addr, SERVO_MAC, sizeof(SERVO_MAC));

  if (esp_now_add_peer(&peer) != ESP_OK) {
    appLog("Servo peer add failed");
  }

  return true;
}

bool espNowReceive(LinkMessage* out) {
  if (rxQueue == NULL || out == NULL) {
    return false;
  }

  return xQueueReceive(rxQueue, out, 0) == pdTRUE;
}

void sendToServo(const char* text) {
  if (text == NULL) {
    return;
  }

  LinkMessage message;

  // Zero first, so the copy below is always terminated.
  memset(&message, 0, sizeof(message));

  strncpy(message.text, text, sizeof(message.text) - 1);

  if (esp_now_send(SERVO_MAC, (const uint8_t*)&message, sizeof(message)) != ESP_OK) {
    appLog("Send to servo failed: %s", message.text);
  }
}
