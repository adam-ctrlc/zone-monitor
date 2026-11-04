// The Adafruit screen libraries are C++, so this file stays C++.

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>

#include "config.h"
#include "display.h"
#include "zones.h"

// Hardware SPI. Listing the MOSI and SCLK pins here instead would make
// the library send every bit from software, which blocks loop() far longer.
static Adafruit_ILI9341 tft(
  &SPI, TFT_DC, TFT_CS, TFT_RST
);

// ---------------- COLORS ----------------
static uint16_t COL_BG;
static uint16_t COL_CARD;
static uint16_t COL_CARD_EDGE;
static uint16_t COL_TEXT;
static uint16_t COL_MUTED;

static uint16_t COL_LV0;
static uint16_t COL_LV1;
static uint16_t COL_LV2;
static uint16_t COL_LV3;

static uint16_t COL_GREEN;
static uint16_t COL_RED;

// ---------------- LAYOUT ----------------
static const int MARGIN = 8;
static const int GAP = 8;

static const int HEADER_H = 26;
static const int FOOTER_H = 18;

static const int COLS = 3;
static const int ROWS = 2;

static const int SCREEN_W = 320;
static const int SCREEN_H = 240;

static int cardW;
static int cardH;

static uint16_t levelColor(uint8_t level) {

  if (level == 0) return COL_LV0;

  if (level == 1) return COL_LV1;

  if (level == 2) return COL_LV2;

  return COL_LV3;
}

static int textWidth(
  const char* s,
  uint8_t size
) {

  return strlen(s) * 6 * size;
}

static void drawHeader() {

  tft.setTextSize(2);

  tft.setTextColor(
    COL_TEXT,
    COL_BG
  );

  tft.setCursor(
    MARGIN,
    4
  );

  tft.print("Zone Monitor");

  const char* sub = "6 LOCATIONS";

  tft.setTextSize(1);

  tft.setTextColor(
    COL_MUTED,
    COL_BG
  );

  tft.setCursor(
    SCREEN_W -
    MARGIN -
    textWidth(sub, 1),
    10
  );

  tft.print(sub);

  tft.drawFastHLine(
    MARGIN,
    HEADER_H,
    SCREEN_W - 2 * MARGIN,
    COL_CARD_EDGE
  );
}

void drawServerStatus(bool online) {

  const int x = 156;

  uint16_t col =
    online ? COL_GREEN : COL_RED;

  tft.fillRect(
    x,
    4,
    88,
    18,
    COL_BG
  );

  tft.fillCircle(
    x + 4,
    13,
    4,
    col
  );

  tft.setTextSize(1);

  tft.setTextColor(
    col,
    COL_BG
  );

  tft.setCursor(
    x + 13,
    9
  );

  tft.print(
    online ? "CONNECTED" : "OFFLINE"
  );
}

static void drawFooter() {

  int y = SCREEN_H - FOOTER_H;

  tft.fillCircle(
    MARGIN + 3,
    y + 8,
    3,
    COL_LV1
  );

  tft.setTextSize(1);

  tft.setTextColor(
    COL_MUTED,
    COL_BG
  );

  tft.setCursor(
    MARGIN + 10,
    y + 3
  );

  tft.print("all zones reporting");
}

static void cardPosition(
  int index,
  int &x,
  int &y
) {

  int col = index % COLS;

  int row = index / COLS;

  x = MARGIN +
      col * (cardW + GAP);

  y = HEADER_H +
      MARGIN +
      row * (cardH + GAP);
}

static void drawCardFrame(int index) {

  int x, y;

  cardPosition(
    index,
    x,
    y
  );

  tft.fillRoundRect(
    x,
    y,
    cardW,
    cardH,
    6,
    COL_CARD
  );

  tft.drawRoundRect(
    x,
    y,
    cardW,
    cardH,
    6,
    COL_CARD_EDGE
  );

  tft.setTextSize(1);

  tft.setTextColor(
    COL_TEXT,
    COL_CARD
  );

  tft.setCursor(
    x + 6,
    y + 8
  );

  tft.print(
    zones[index].name
  );
}

void drawCardLevel(int index) {

  if (index < 0 || index >= ZONE_COUNT) {

    return;
  }

  int x, y;

  cardPosition(
    index,
    x,
    y
  );

  uint16_t accent =
    levelColor(zones[index].level);

  tft.fillRect(
    x + 2,
    y + 1,
    cardW - 4,
    2,
    accent
  );

  tft.fillCircle(
    x + cardW - 8,
    y + 11,
    3,
    accent
  );

  tft.setTextSize(3);

  tft.setTextColor(
    accent,
    COL_CARD
  );

  tft.setCursor(
    x + 6,
    y + 22
  );

  tft.print(
    zones[index].level
  );

  int segY =
    y + cardH - 12;

  int segGap = 3;

  int segW =
    (cardW - 12 - 2 * segGap) / 3;

  for (int s = 0; s < 3; s++) {

    int segX =
      x + 6 +
      s * (segW + segGap);

    uint16_t segColor =
      (s < zones[index].level)
      ? accent
      : COL_CARD_EDGE;

    tft.fillRoundRect(
      segX,
      segY,
      segW,
      5,
      2,
      segColor
    );
  }
}

static void drawGrid() {

  for (int i = 0; i < ZONE_COUNT; i++) {

    drawCardFrame(i);

    drawCardLevel(i);
  }
}

void drawEstopBanner() {

  int y =
    SCREEN_H - FOOTER_H;

  tft.fillRect(
    0,
    y,
    SCREEN_W,
    FOOTER_H,
    COL_LV3
  );

  tft.setTextSize(1);

  tft.setTextColor(
    COL_BG,
    COL_LV3
  );

  tft.setCursor(
    MARGIN,
    y + 5
  );

  tft.print(
    "EMERGENCY STOP - resume from website"
  );
}

void clearEstopBanner() {

  int y =
    SCREEN_H - FOOTER_H;

  tft.fillRect(
    0,
    y,
    SCREEN_W,
    FOOTER_H,
    COL_BG
  );

  drawFooter();
}

void displaySetup() {

  // Must run before tft.begin(), which keeps these pins.
  SPI.begin(
    TFT_SCLK,
    -1,
    TFT_MOSI,
    -1
  );

  tft.begin(TFT_SPI_HZ);

  tft.setRotation(1);

  COL_BG = tft.color565(
    0x0c, 0x0f, 0x15
  );

  COL_CARD = tft.color565(
    0x1b, 0x21, 0x2c
  );

  COL_CARD_EDGE = tft.color565(
    0x26, 0x2e, 0x3b
  );

  COL_TEXT = tft.color565(
    0xe9, 0xed, 0xf3
  );

  COL_MUTED = tft.color565(
    0x7c, 0x87, 0x9a
  );

  COL_LV0 = tft.color565(
    0x4a, 0x53, 0x63
  );

  COL_LV1 = tft.color565(
    0x4f, 0xd1, 0xc5
  );

  COL_LV2 = tft.color565(
    0xf6, 0xad, 0x55
  );

  COL_LV3 = tft.color565(
    0xfc, 0x81, 0x81
  );

  COL_GREEN = tft.color565(
    0x3d, 0xdc, 0x84
  );

  COL_RED = tft.color565(
    0xf5, 0x3d, 0x3d
  );

  cardW =
    (SCREEN_W -
     2 * MARGIN -
     (COLS - 1) * GAP) / COLS;

  cardH =
    (SCREEN_H -
     HEADER_H -
     FOOTER_H -
     2 * MARGIN -
     (ROWS - 1) * GAP) / ROWS;

  tft.fillScreen(COL_BG);

  drawHeader();

  drawServerStatus(false);

  drawFooter();

  drawGrid();
}
