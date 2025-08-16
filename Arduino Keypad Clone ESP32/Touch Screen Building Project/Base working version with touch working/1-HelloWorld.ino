#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <TFT_eSPI.h>

TFT_eSPI tft;

// CYD touch pins
static const int PIN_TOUCH_CS   = 33;
static const int PIN_TOUCH_IRQ  = 36;
static const int PIN_TOUCH_MOSI = 32;
static const int PIN_TOUCH_MISO = 39;
static const int PIN_TOUCH_SCLK = 25;

SPIClass touchSPI(HSPI);
XPT2046_Touchscreen ts(PIN_TOUCH_CS, PIN_TOUCH_IRQ);

// ---- Calibration (update after you print raw corners) ----
int16_t rxMin = 300,  rxMax = 3800;
int16_t ryMin = 300,  ryMax = 3800;

// ---- Orientation controls ----
// Start with these, then only change TS_ROT first (0..3).
uint8_t TS_ROT = 1;     // rotation for the touch controller: try 0,1,2,3
bool    FLIP_X = false; // set true if left/right is mirrored AFTER getting axes correct
bool    FLIP_Y = false; // set true if up/down is mirrored AFTER getting axes correct

void setup() {
  Serial.begin(115200);

  // Display
  tft.init();
  tft.setRotation(1);       // 320x240, origin top-left
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Touch test: draw", 10, 10, 2);

  // Touch on its own SPI bus
  touchSPI.begin(PIN_TOUCH_SCLK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(TS_ROT);   // let library handle axis swap/rotation first
}

static void mapToScreen(int32_t rx, int32_t ry, int16_t &sx, int16_t &sy) {
  // Optional flips AFTER rotation
  int16_t x0 = FLIP_X ? rxMax : rxMin;
  int16_t x1 = FLIP_X ? rxMin : rxMax;
  int16_t y0 = FLIP_Y ? ryMax : ryMin;
  int16_t y1 = FLIP_Y ? ryMin : ryMax;

  sx = map(rx, x0, x1, 0, 319);
  sy = map(ry, y0, y1, 0, 239);
}

void loop() {
  if (!ts.touched()) return;

  TS_Point p = ts.getPoint();     // already rotated per TS_ROT

  // print occasionally so you can refine rx/ry min/max from corners
  static uint32_t last=0;
  if (millis()-last > 400) {
    Serial.printf("raw after rot: x=%d y=%d z=%d (rot=%u flipX=%d flipY=%d)\n",
                  p.x, p.y, p.z, TS_ROT, FLIP_X, FLIP_Y);
    last = millis();
  }

  int16_t sx, sy;
  mapToScreen(p.x, p.y, sx, sy);

  if (sx>=0 && sx<320 && sy>=0 && sy<240) {
    tft.fillCircle(sx, sy, 3, TFT_YELLOW);
  }
}
