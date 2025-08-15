#include <Arduino.h>

// 1) Tell LVGL to look in the sketch folder for lv_conf.h
#define LV_CONF_INCLUDE_SIMPLE
#include "lv_conf.h"

// 2) Use local per-project TFT_eSPI setup
#define USER_SETUP_LOADED
#include "User_Setup.h"

#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <XPT2046_Touchscreen.h>

// 3) SquareLine headers are C, wrap them:
extern "C" {
  #include "ui.h"
}
// ---- TFT ----
TFT_eSPI tft;

// ---- Touch (CYD pins on HSPI) ----
static const int PIN_TOUCH_CS   = 33;
static const int PIN_TOUCH_IRQ  = 36;  // optional
static const int PIN_TOUCH_MOSI = 32;
static const int PIN_TOUCH_MISO = 39;
static const int PIN_TOUCH_SCLK = 25;

SPIClass touchSPI(HSPI);
XPT2046_Touchscreen ts(PIN_TOUCH_CS, PIN_TOUCH_IRQ);

// ---- Screen size ----
static const uint16_t SCREEN_W = 320;
static const uint16_t SCREEN_H = 240;

// ---- LVGL draw buffer ----
static lv_color_t buf1[SCREEN_W * 40];   // ~40 lines; raise if you want faster draws
static lv_disp_draw_buf_t draw_buf;

// ---- Touch calibration/orientation ----
// (tweak after testing; these are safe starters)
static int16_t rxMin = 300,  rxMax = 3800;
static int16_t ryMin = 300,  ryMax = 3800;
static uint8_t TS_ROT = 1;          // try 0..3 until axes match
static bool    FLIP_X = false;      // set true if right/left swapped
static bool    FLIP_Y = false;      // set true if down/up swapped

// ---- LVGL flush using TFT_eSPI ----
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  int32_t x1 = area->x1, y1 = area->y1, x2 = area->x2, y2 = area->y2;
  tft.startWrite();
  tft.setAddrWindow(x1, y1, (x2 - x1 + 1), (y2 - y1 + 1));
  tft.pushColors((uint16_t *)&color_p->full, (x2 - x1 + 1) * (y2 - y1 + 1), true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

// ---- Touch read for LVGL ----
static void mapToScreen(int32_t rx, int32_t ry, int16_t &sx, int16_t &sy) {
  int16_t x0 = FLIP_X ? rxMax : rxMin;
  int16_t x1 = FLIP_X ? rxMin : rxMax;
  int16_t y0 = FLIP_Y ? ryMax : ryMin;
  int16_t y1 = FLIP_Y ? ryMin : ryMax;
  sx = map(rx, x0, x1, 0, SCREEN_W - 1);
  sy = map(ry, y0, y1, 0, SCREEN_H - 1);
}

static void my_touch_read(lv_indev_drv_t * indev, lv_indev_data_t * data) {
  if (!ts.touched()) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }
  TS_Point p = ts.getPoint();   // rotated by TS_ROT
  int16_t sx, sy;
  mapToScreen(p.x, p.y, sx, sy);
  data->state   = LV_INDEV_STATE_PR;
  data->point.x = sx;
  data->point.y = sy;
}

void setup() {
  Serial.begin(115200);

  // TFT
  tft.init();
  tft.setRotation(1);          // landscape 320x240
  tft.fillScreen(TFT_BLACK);

  // Touch
  touchSPI.begin(PIN_TOUCH_SCLK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  ts.begin(touchSPI);
  ts.setRotation(TS_ROT);      // fix axes here first

  // LVGL
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_W * 40);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_W;
  disp_drv.ver_res = SCREEN_H;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touch_read;
  lv_indev_drv_register(&indev_drv);

  // Start your SquareLine screens
  ui_init();
}

void loop() {
  lv_timer_handler();
  delay(5);
}
