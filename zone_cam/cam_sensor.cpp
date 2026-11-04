#include "esp_camera.h"
#include "img_converters.h"

#include "cam_config.h"
#include "cam_sensor.h"

bool camOk = false;

static uint8_t* previewBuf = NULL;

static SemaphoreHandle_t previewMutex = NULL;

// Written by the QR task on core 1, read by the website task on core 0.
static volatile unsigned long lastPreview = 0;

static volatile unsigned long lastPreviewRequest = 0;

void flashSetup() {

  pinMode(
    FLASH_PIN,
    OUTPUT
  );

  flashSet(false);
}

void flashSet(bool on) {

  digitalWrite(
    FLASH_PIN,
    on ? HIGH : LOW
  );
}

void cameraSetup() {

  // ---------------------------------------------------
  // PREVIEW MEMORY
  // ---------------------------------------------------

  previewMutex = xSemaphoreCreateMutex();

  if (previewMutex == NULL) {

    Serial.println(
      "Preview mutex creation failed"
    );
  }

  previewBuf = (uint8_t*)ps_calloc(
    1,
    FRAME_W * FRAME_H * BYTES_PER_PIXEL
  );

  // ---------------------------------------------------
  // CAMERA CONFIGURATION
  // ---------------------------------------------------

  camera_config_t config;

  memset(
    &config,
    0,
    sizeof(config)
  );

  config.ledc_channel = LEDC_CHANNEL_0;

  config.ledc_timer = LEDC_TIMER_0;

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  config.pin_xclk = XCLK_GPIO_NUM;

  config.pin_pclk = PCLK_GPIO_NUM;

  config.pin_vsync = VSYNC_GPIO_NUM;

  config.pin_href = HREF_GPIO_NUM;

  config.pin_sscb_sda = SIOD_GPIO_NUM;

  config.pin_sscb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;

  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;

  config.pixel_format = PIX_FORMAT;

  config.frame_size = FRAME_SIZE;

  config.jpeg_quality = 15;

  config.fb_count = 2;

  config.fb_location = CAMERA_FB_IN_PSRAM;

  config.grab_mode = CAMERA_GRAB_LATEST;

  if (previewBuf == NULL) {

    Serial.println(
      "PSRAM allocation failed"
    );

  } else if (
    esp_camera_init(&config) != ESP_OK
  ) {

    Serial.println(
      "Camera initialization failed"
    );

  } else {

    camOk = true;

    Serial.println(
      "Camera initialized successfully"
    );
  }

  // ---------------------------------------------------
  // CAMERA IMAGE SETTINGS
  // ---------------------------------------------------

  if (camOk) {

    sensor_t* s = esp_camera_sensor_get();

    s->set_brightness(s, 1);

    s->set_contrast(s, 2);

    s->set_saturation(s, 0);

    s->set_whitebal(s, 1);

    s->set_awb_gain(s, 1);

    // Fixed exposure and gain. Automatic exposure lets
    // the shutter stay open longer, which blurs a moving QR.
    s->set_exposure_ctrl(s, 0);

    s->set_aec2(s, 0);

    s->set_aec_value(s, CAM_EXPOSURE);

    s->set_gain_ctrl(s, 0);

    s->set_agc_gain(s, CAM_GAIN);

    s->set_bpc(s, 1);

    s->set_wpc(s, 1);

    s->set_raw_gma(s, 1);

    s->set_lenc(s, 1);

    s->set_dcw(s, 1);

    Serial.println(
      "Camera image settings applied"
    );
  }
}

void previewUpdate(camera_fb_t* fb) {

  if (
    millis() - lastPreviewRequest < PREVIEW_ACTIVE_MS &&
    millis() - lastPreview > 300
  ) {

    if (
      xSemaphoreTake(
        previewMutex,
        0
      ) == pdTRUE
    ) {

      memcpy(
        previewBuf,
        fb->buf,
        fb->len
      );

      xSemaphoreGive(previewMutex);

      lastPreview = millis();
    }
  }
}

void previewRequest() {

  lastPreviewRequest = millis();
}

bool previewToJpg(uint8_t** jpg, size_t* jpgLen) {

  lastPreviewRequest = millis();

  // The copy may be old if nobody was watching.
  if (
    lastPreview == 0 ||
    millis() - lastPreview > 1000
  ) {

    return false;
  }

  bool ok = false;

  if (
    camOk &&
    previewMutex != NULL &&
    xSemaphoreTake(
      previewMutex,
      pdMS_TO_TICKS(300)
    ) == pdTRUE
  ) {

    ok = fmt2jpg(
      previewBuf,
      FRAME_W * FRAME_H * BYTES_PER_PIXEL,
      FRAME_W,
      FRAME_H,
      PIX_FORMAT,
      60,
      jpg,
      jpgLen
    );

    xSemaphoreGive(previewMutex);
  }

  return ok;
}

bool cameraTune(bool hasAec, int aec, bool hasGain, int gain) {

  sensor_t* s = camOk ? esp_camera_sensor_get() : NULL;

  if (s == NULL) {

    return false;
  }

  if (hasAec) {

    s->set_aec_value(
      s,
      constrain(aec, 0, 1200)
    );
  }

  if (hasGain) {

    s->set_agc_gain(
      s,
      constrain(gain, 0, 30)
    );
  }

  return true;
}
