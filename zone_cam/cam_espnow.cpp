#include <WiFi.h>
#include <esp_now.h>

#include "cam_config.h"
#include "cam_espnow.h"

// Keep the same message structure as your main controller.
typedef struct {
  char text[32];
} Message;

static void sendMessage(const uint8_t* receiver, const char* text) {

  Message msg;

  // Zero first, so the copy below is always terminated.
  memset(&msg, 0, sizeof(msg));

  strncpy(
    msg.text,
    text,
    sizeof(msg.text) - 1
  );

  esp_err_t result = ESP_FAIL;

  // Normally sends at once. Waits only if the send queue is
  // full, and never for long, so scanning is not held up.
  for (int attempt = 0; attempt < 5; attempt++) {

    result = esp_now_send(
      receiver,
      (const uint8_t*)&msg,
      sizeof(msg)
    );

    if (result != ESP_ERR_ESPNOW_NO_MEM) {

      break;
    }

    vTaskDelay(pdMS_TO_TICKS(2));
  }

  if (result != ESP_OK) {

    Serial.print("ESP-NOW send error: ");

    Serial.println(result);
  }
}

void sendDestination(const char* destination) {

  if (destination == NULL || destination[0] == '\0') {

    return;
  }

  // Send destination to both controllers.
  sendMessage(
    receiverMAC1,
    destination
  );

  sendMessage(
    receiverMAC2,
    destination
  );

  // The S3 also treats the destination itself as a read,
  // so this one message is the backup if that is missed.
  sendMessage(
    receiverMAC1,
    "QR_CLEARED"
  );

  Serial.print("QR FOUND: ");

  Serial.println(destination);
}

void sendQrSeen() {

  sendMessage(
    receiverMAC1,
    "QR_SEEN"
  );
}

void espNowSetup() {

  if (esp_now_init() != ESP_OK) {

    Serial.println(
      "ESP-NOW initialization failed"
    );

  } else {

    esp_now_peer_info_t peerInfo = {};

    peerInfo.channel = 0;

    peerInfo.encrypt = false;

    peerInfo.ifidx = WIFI_IF_STA;

    // Add ESP32-S3 main controller.
    memcpy(
      peerInfo.peer_addr,
      receiverMAC1,
      6
    );

    if (
      esp_now_add_peer(&peerInfo) == ESP_OK
    ) {

      Serial.println(
        "ESP32-S3 peer added"
      );

    } else {

      Serial.println(
        "ESP32-S3 peer add failed"
      );
    }

    // Add standard ESP32 servo controller.
    memcpy(
      peerInfo.peer_addr,
      receiverMAC2,
      6
    );

    if (
      esp_now_add_peer(&peerInfo) == ESP_OK
    ) {

      Serial.println(
        "Servo controller peer added"
      );

    } else {

      Serial.println(
        "Servo controller peer add failed"
      );
    }
  }
}
