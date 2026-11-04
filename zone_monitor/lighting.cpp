#include <FastLED.h>

#include "config.h"
#include "lighting.h"

static CRGB leds[NUM_LEDS];

static uint32_t lastRefresh = 0;

void lightingSetup() {

  FastLED.addLeds<
    LED_TYPE,
    LED_PIN,
    COLOR_ORDER
  >(leds, NUM_LEDS);

  FastLED.setBrightness(BRIGHTNESS);

  fill_solid(leds, NUM_LEDS, CRGB::White);

  FastLED.show();
}

void lightingUpdate() {

  if (millis() - lastRefresh >= REFRESH_MS) {

    lastRefresh = millis();

    FastLED.show();
  }
}

void lightingSetEstop(bool on) {

  fill_solid(
    leds,
    NUM_LEDS,
    on ? CRGB::Red : CRGB::White
  );

  FastLED.show();
}
