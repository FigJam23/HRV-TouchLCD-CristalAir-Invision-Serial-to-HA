/***********************
 *  DISPLAY + TOUCH (CYD) + HRV + SHT31 + Web UI
 ***********************/
#include <Arduino.h>

#define LV_CONF_INCLUDE_SIMPLE
#include "lv_conf.h"

#define USER_SETUP_LOADED
#include "User_Setup.h"

#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <vector>
#include <math.h>
#include <Preferences.h>   // NVS persistence

// --- Web UI / FS / mDNS ---
#include <FS.h>
using fs::FS;
#include <WebServer.h>
#include <ESPmDNS.h>

// --- SHT31 ---
#include <Wire.h>
#include <Adafruit_SHT31.h>

// SquareLine
#include "ui.h"

// If you already have a setpoint slider/label in LVGL, define this:
// #define UI_HAS_SETPOINT

/***********************
 *  WIFI + MQTT CONFIG
 ***********************/
static const char* WIFI_SSID     = "Reaver";
static const char* WIFI_PASSWORD = "Figjam";

static const char* AP_SSID       = "HRV-Keypad";
static const char* AP_PASS       = "12345678";  // 8+ chars required by ESP

// Compiled-in MQTT defaults (used on first boot, then overridden by NVS)
static const char* MQTT_HOST     = "192.168.1.44";
static const uint16_t MQTT_PORT  = 1883;
static const char* MQTT_USER     = "mqtt";
static const char* MQTT_PASS     = "Just";
static const char* MQTT_CLIENTID = "hrv";

// Mutable (runtime/NVS) MQTT settings
String   mqtt_host  = MQTT_HOST;
uint16_t mqtt_port  = MQTT_PORT;
String   mqtt_user  = MQTT_USER;
String   mqtt_pass  = MQTT_PASS;

static const char* T_FILTER_DAYS    = "hassio/hrv/filter_days_remaining/state";
static const char* T_FILTER_LIFE    = "hassio/hrv/filter_life/state";
static const char* T_FAN_PERCENT    = "hassio/hrv/fan_percent/state";
static const char* T_BOOST_ACTIVE   = "hassio/hrv/boost_active/state";
static const char* T_FILTER_NEEDED  = "hassio/hrv/filter_replacement_needed";
static const char* T_HOUSE_TEMP     = "hassio/hrv/house_temp/state";
static const char* T_HOUSE_HUM      = "hassio/hrv/house_humidity/state";
static const char* T_SETPOINT_STATE = "hassio/hrv/setpoint/state"; // retained
static const char* T_SETPOINT_CMD   = "hassio/hrv/setpoint/set";   // retained command (optional from HA)

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

/***********************
 *  NTP / TIME ZONE (NZ, with DST)
 ***********************/
static const char* TIMEZONE = "NZST-12NZDT,M9.5.0/2,M4.1.0/3";
static const char* NTP1 = "pool.ntp.org";
static const char* NTP2 = "time.nist.gov";
static const char* NTP3 = "time.google.com";
static uint32_t last_clock_ms = 0;

/***********************
 *  TFT/TOUCH
 ***********************/
TFT_eSPI tft;

// XPT2046 on HSPI (CYD)
static const int PIN_TOUCH_CS   = 33;
static const int PIN_TOUCH_IRQ  = 36;
static const int PIN_TOUCH_MOSI = 32;
static const int PIN_TOUCH_MISO = 39;
static const int PIN_TOUCH_SCLK = 25;
static const int PIN_TFT_BL     = 21;

// --- BUZZER: 2-pin header on back of screen ---
static const int BUZZER_PIN = 26;   // <--- set this to your buzzer pin
// software-PWM beep (portable, non-blocking)
static bool     buzzer_ready = false;
static bool     beep_active  = false;
static uint32_t beep_end_ms  = 0;
static uint32_t beep_next_toggle_us = 0;
static uint32_t beep_half_period_us = 0;
static bool     beep_pin_state = false;

static inline void buzzer_init() {
  if (BUZZER_PIN < 0) return;
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  buzzer_ready = true;
}
static inline void beep_now(uint16_t ms = 30, uint16_t freq = 2500) {
  if (!buzzer_ready) return;
  if (freq < 50) freq = 50;
  beep_half_period_us = 500000UL / (uint32_t)freq;  // half period
  beep_next_toggle_us = micros();
  beep_end_ms = millis() + ms;
  beep_pin_state = false;
  digitalWrite(BUZZER_PIN, LOW);
  beep_active = true;
}
static inline void beep_service() {
  if (!beep_active) return;
  if ((int32_t)(millis() - beep_end_ms) >= 0) {
    digitalWrite(BUZZER_PIN, LOW);
    beep_active = false;
    return;
  }
  uint32_t nowus = micros();
  if ((int32_t)(nowus - beep_next_toggle_us) >= 0) {
    beep_pin_state = !beep_pin_state;
    digitalWrite(BUZZER_PIN, beep_pin_state ? HIGH : LOW);
    beep_next_toggle_us = nowus + beep_half_period_us;
  }
}

SPIClass touchSPI(HSPI);
XPT2046_Touchscreen ts(PIN_TOUCH_CS, PIN_TOUCH_IRQ);

static const uint16_t SCREEN_W = 240;
static const uint16_t SCREEN_H = 320;

static lv_color_t buf1[SCREEN_W * 40];
static lv_disp_draw_buf_t draw_buf;

static int16_t rxMin = 300,  rxMax = 3800;
static int16_t ryMin = 300,  ryMax = 3800;
static uint8_t TS_ROT = 0;
static bool    FLIP_X = false;
static bool    FLIP_Y = false;

// --- UI guards ---
static bool ui_prog = false;        // we change slider in code
static bool slider_drag = false;    // user is dragging
static uint32_t ui_reflect_block_until = 0;  // grace window after release
static inline bool ui_reflect_blocked() {
  return slider_drag || (int32_t)(millis() - ui_reflect_block_until) < 0;
}

/***********************
 *  HRV UART  (half-duplex bus on GPIO3/1)
 ***********************/
#define HRV_RX_PIN 3
#define HRV_TX_PIN 1
HardwareSerial HRV(2);
static uint32_t last_tx_ms = 0;
static uint32_t last_rx_any_ms = 0;

/***********************
 *  SHT31 on I²C
 ***********************/
static const int PIN_I2C_SDA = 27;
static const int PIN_I2C_SCL = 22;
Adafruit_SHT31 sht31 = Adafruit_SHT31();
static bool sht_ok = false;
static uint32_t last_sht_ms = 0;
static const uint32_t SHT_PERIOD_MS = 30000;

/***********************
 *  STATE
 ***********************/
static float  last_house_temp = 255;   // 255 = unknown
static float  last_roof_temp  = 255;
static float  last_house_hum  = NAN;
static int    last_fan_speed  = 255;

static uint32_t ts_house = 0, ts_roof = 0, ts_fan = 0;
static uint32_t sensor_timeout_ms = 15000;

static int   hrv_days_remaining  = 730;
static int   target_percent      = 30;  // default target holder
static int   last_nonzero_percent= 30;
static int   prev_target_percent = 30;

static bool  power_off           = false;
static bool  boost_active        = false;           // Burnt Toast
static uint32_t boost_duration_ms= 5UL * 60UL * 1000UL;
static uint32_t boost_end_ms     = 0;

static float setpoint_c          = 18.0f;

static uint32_t last_sent_ms     = 0;

/******** Auto/Setpoint controller ********/
static bool  auto_mode = true;                    // start in AUTO now
static uint32_t manual_hold_until_ms = 0;         // manual override window
static const uint32_t MANUAL_HOLD_MS = 20UL*60UL*1000UL; // 20 min

// Tunables
static const float SP_TOL_C            = 0.5f;    // ±0.5°C -> hold/trickle
static const float ROOF_HELP_DEADBAND  = 0.3f;    // roof must be this much "better"
static const int   MIN_TRICKLE         = 10;      // %
static const int   MIN_HELP_SPEED      = 20;      // %
static const float GAIN_PCT_PER_C      = 12.0f;   // % per °C beyond deadband
static const int   NEAR_SP_MAX         = 30;      // cap when near target
static const int   UNKNOWN_FAN_FALLBACK= 30;      // % when temps unknown (matches post-toast)
static int         last_auto_percent   = -1;      // force first auto push

/***********************
 *  Persistence (NVS)
 ***********************/
Preferences prefs;
static const char* NVS_NS = "hrv";
static const char* KEY_SETPOINT = "setpoint";
static const char* KEY_MQTT_HOST = "mqtt_host";
static const char* KEY_MQTT_PORT = "mqtt_port";
static const char* KEY_MQTT_USER = "mqtt_user";
static const char* KEY_MQTT_PASS = "mqtt_pass";

/***********************
 *  WIFI / MQTT (non-blocking)
 ***********************/
static bool ap_mode = false;
static bool web_started = false;
static bool mdns_started = false;
static uint32_t wifi_begin_ms = 0;
static uint32_t next_mqtt_try_ms = 0;

/***********************
 *  WEB UI
 ***********************/
WebServer web(80);

// forward decls
static void handle_setpoint();
static void handle_auto();
static void poll_sht31();  // ensure declared before use
static void handle_mqtt_cfg();

static String current_mode() {
  if (power_off)    return "Off";
  if (boost_active) return "Burnt Toast";
  if (target_percent <= 12) return "Trickle";

  if (last_house_temp != 255 && last_roof_temp != 255) {
    float err = setpoint_c - last_house_temp;
    if (fabsf(err) <= SP_TOL_C) return "Hold";
    float roof_delta = last_roof_temp - last_house_temp;
    bool heating = err > 0;
    bool helpful = (heating && roof_delta >  ROOF_HELP_DEADBAND) ||
                   (!heating && roof_delta < -ROOF_HELP_DEADBAND);
    if (helpful) return heating ? "Heating" : "Cooling";
  }
  return "Ventilate";
}

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset=utf-8><meta name=viewport content="width=device-width,initial-scale=1">
<title>HRV Keypad</title>
<style>
body{font-family:system-ui,Arial;margin:16px} .row{margin:8px 0} .val{font-weight:600}
.btn{padding:8px 12px;margin:4px;border:1px solid #888;border-radius:6px;cursor:pointer;background:#eee}
.range{width:100%} .ok{color:#0a0} .bad{color:#c00}
input,button{font:inherit}
fieldset{border:1px solid #ccc;border-radius:8px;padding:8px 12px}
legend{padding:0 6px;color:#555}
label{display:block;margin:6px 0 2px}
</style></head>
<body>
<h2>HRV Keypad</h2>
<div class=row>IP: <span id=ip class=val></span> | RSSI: <span id=rssi class=val></span> dBm | WiFi: <span id=wf class=val></span></div>
<div class=row>Time: <span id=time class=val></span> | Mode: <span id=mode class=val></span></div>
<div class=row>House: <span id=house class=val></span> °C | Hum: <span id=hum class=val></span> % | Roof: <span id=roof class=val></span> °C</div>
<div class=row>Fan Target: <span id=fanT class=val></span>% | Actual: <span id=fanA class=val></span>%</div>
<div class=row>Filter: <span id=filter class=val></span> (<span id=filterPct></span>%)</div>
<div class=row>
  <button class=btn id=powerBtn>Toggle Power</button>
  <button class=btn id=boostBtn>Burnt Toast</button>
</div>
<div class=row>
  <input id=fanRange class=range type=range min=0 max=100 step=1>
</div>

<details class=row open>
  <summary><b>MQTT Settings</b></summary>
  <fieldset>
    <legend>Broker</legend>
    <label>Host/IP</label><input id=mqtthost placeholder="host">
    <label>Port</label><input id=mqttport type=number min=1 max=65535 placeholder="1883">
  </fieldset>
  <fieldset>
    <legend>Auth</legend>
    <label>User</label><input id=mqttuser placeholder="user">
    <label>Password</label><input id=mqttpass type=password placeholder="password">
  </fieldset>
  <div class=row>
    <button class=btn id=mqttSave>Save & Reconnect</button>
    <span id=mqttMsg></span>
  </div>
</details>

<script>
async function q(p){return fetch(p).then(r=>r.json())}
function g(id){return document.getElementById(id)}

async function loadMQTT(){
  try{
    const m=await q('/mqtt');
    g('mqtthost').value=m.host||'';
    g('mqttport').value=m.port||'1883';
    g('mqttuser').value=m.user||'';
    g('mqttpass').value=m.pass||'';
  }catch(e){}
}

async function saveMQTT(){
  const host=encodeURIComponent(g('mqtthost').value.trim());
  const port=encodeURIComponent(g('mqttport').value.trim());
  const user=encodeURIComponent(g('mqttuser').value.trim());
  const pass=encodeURIComponent(g('mqttpass').value);
  try{
    const r=await q(`/mqtt?host=${host}&port=${port}&user=${user}&pass=${pass}`);
    g('mqttMsg').textContent=r.ok?'Saved. Reconnecting…':'Failed';
    setTimeout(()=>g('mqttMsg').textContent='',2000);
  }catch(e){g('mqttMsg').textContent='Error';}
}
g('mqttSave').onclick=saveMQTT;

async function tick(){
  try{
    const s=await q('/status');
    g('ip').textContent=s.ip; g('rssi').textContent=s.rssi; g('wf').textContent=s.wifi_mode;
    g('time').textContent=s.time; g('mode').textContent=s.mode;
    g('house').textContent=(s.house===null?'--':s.house.toFixed(1));
    g('hum').textContent  =(s.hum===null?'--':s.hum.toFixed(1));
    g('roof').textContent =(s.roof===null?'--':s.roof.toFixed(1));
    g('fanT').textContent=s.fan_target; g('fanA').textContent=s.fan_actual;
    g('filter').textContent=s.filter_text; g('filter').className=s.filter_text=='REPLACE'?'val bad':'val ok';
    g('filterPct').textContent=s.filter_pct;

    const fr=g('fanRange');
    fr.disabled = s.slider_disabled;
    if(!fr.dragging) fr.value=s.fan_target;

    g('boostBtn').textContent=s.boost?'Burnt Toast (ON)':'Burnt Toast (OFF)';
    g('boostBtn').disabled=s.power_off;
  }catch(e){}
}

g('powerBtn').onclick=()=>q('/api/power?toggle=1');
g('boostBtn').onclick =()=>q('/api/boost?toggle=1');

const fr=g('fanRange');
fr.oninput =()=>{fr.dragging=true; g('fanT').textContent=fr.value}
fr.onchange=()=>{fetch('/api/setfan?val='+fr.value); fr.dragging=false}

loadMQTT();
setInterval(tick,1000); tick();
</script>
</body></html>
)HTML";

/***********************
 *  UI helpers
 ***********************/
static void ui_set_label_text(lv_obj_t* lbl, const String& s){
  if (!lbl) return; static char buf[32]; s.toCharArray(buf,sizeof(buf)); lv_label_set_text(lbl, buf);
}
static void set_filter_text_by_pct(int lifePct){
  if (ui_Filter_Life_Value) ui_set_label_text(ui_Filter_Life_Value, (lifePct<=0) ? "REPLACE" : "OK");
}
static void set_slider_enabled(bool en){
  if (!ui_Fan_Speed_Slider) return;
  if (en) lv_obj_clear_state(ui_Fan_Speed_Slider, LV_STATE_DISABLED);
  else    lv_obj_add_state(ui_Fan_Speed_Slider,  LV_STATE_DISABLED);
}
static void set_boost_button_enabled(bool en){
  if (!ui_Burnt_Toast) return;
  if (en) lv_obj_clear_state(ui_Burnt_Toast, LV_STATE_DISABLED);
  else    lv_obj_add_state(ui_Burnt_Toast,  LV_STATE_DISABLED);
}
static void set_setpoint_enabled(bool en){
#ifdef UI_HAS_SETPOINT
  if (ui_Setpoint_Slider){
    if (en) lv_obj_clear_state(ui_Setpoint_Slider, LV_STATE_DISABLED);
    else    lv_obj_add_state(ui_Setpoint_Slider,  LV_STATE_DISABLED);
  }
#endif
}
static void reflect_setpoint_ui(){
#ifdef UI_HAS_SETPOINT
  if (ui_Setpoint_Value) ui_set_label_text(ui_Setpoint_Value, String(setpoint_c,1));
#endif
}
static void reflect_fan_percent_on_ui(int p){
  if (ui_reflect_blocked()) return;
  if (ui_Fan_Speed_Slider) {
    int cur = lv_slider_get_value(ui_Fan_Speed_Slider);
    if (abs(cur - p) >= 2) { // hysteresis
      ui_prog = true;
      lv_slider_set_value(ui_Fan_Speed_Slider, p, LV_ANIM_OFF);
      ui_prog = false;
    }
  }
  if (ui_Fan_Speed_Value) ui_set_label_text(ui_Fan_Speed_Value, String(p));
}

/***********************
 *  DISPLAY / TOUCH
 ***********************/
static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p){
  const int32_t w = (area->x2 - area->x1 + 1);
  const int32_t h = (area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
#if LV_COLOR_16_SWAP
  tft.pushColors((uint16_t*)&color_p->full, w*h, false);
#else
  tft.pushColors((uint16_t*)&color_p->full, w*h, true);
#endif
  tft.endWrite();
  lv_disp_flush_ready(disp);
}
static void mapToScreen(int32_t rx,int32_t ry,int16_t &sx,int16_t &sy){
  int16_t x0 = FLIP_X ? rxMax : rxMin;
  int16_t x1 = FLIP_X ? rxMin : rxMax;
  int16_t y0 = FLIP_Y ? ryMax : ryMin;
  int16_t y1 = FLIP_Y ? ryMin : ryMax;
  sx = map(rx, x0, x1, 0, SCREEN_W - 1);
  sy = map(ry, y0, y1, 0, SCREEN_H - 1);
}
static void my_touch_read(lv_indev_drv_t * indev, lv_indev_data_t * data){
  if (!ts.touched()) { data->state = LV_INDEV_STATE_REL; return; }
  TS_Point p = ts.getPoint();
  int16_t sx, sy; mapToScreen(p.x, p.y, sx, sy);
  data->state = LV_INDEV_STATE_PR; data->point.x = sx; data->point.y = sy;
}

/***********************
 *  HRV BUS helpers
 ***********************/
static void send_frame(const uint8_t* body, size_t n){
  uint32_t gap = millis() - last_tx_ms; if (gap < 140) delay(140 - gap);
  uint8_t sum = 0; for (size_t i=0;i<n;i++) sum -= body[i];
  HRV.write(0x7E); HRV.write(body, n); HRV.write(sum); HRV.write(0x7E); HRV.flush();
  last_tx_ms = millis();
}
static void send2(uint8_t b1,uint8_t b2){ uint8_t b[]={b1,b2}; send_frame(b,sizeof(b)); }
static void send4(uint8_t b1,uint8_t b2,uint8_t b3,uint8_t b4){ uint8_t b[]={b1,b2,b3,b4}; send_frame(b,sizeof(b)); }
static void send6(uint8_t b1,uint8_t b2,uint8_t b3,uint8_t b4,uint8_t b5,uint8_t b6){ uint8_t b[]={b1,b2,b3,b4,b5,b6}; send_frame(b,sizeof(b)); }

static void send_fan_now(int p){
  p = constrain(p, 0, 100);
  uint8_t code = (p<=30) ? 0x4E : (p<=75 ? 0x4F : 0x50);
  uint8_t body[] = {0x31,0x01,code,(uint8_t)p,0x1E,0x84,0xF0};
  send_frame(body,sizeof(body));
  last_sent_ms = millis(); ts_fan = last_sent_ms;
  reflect_fan_percent_on_ui(p);
  if (mqtt.connected()) mqtt.publish(T_FAN_PERCENT, String(p).c_str(), true);
}

/***********************
 *  HRV handshake
 ***********************/
static void boot_handshake(){
  send4(0x36,0x00,0x00,0x00); delay(200);
  send4(0x36,0x00,0x00,0x00); delay(200);
  send4(0x36,0x00,0x00,0x00); delay(200);
  send4(0x36,0x00,0x00,0x00); delay(250);
  send6(0x37,0x01,0x6A,0x00,0x1E,0x84); delay(180);
  send2(0x34,0xE3); delay(120);
  send2(0x33,0xC3); delay(120);
  send2(0x43,0xC3); delay(120);
  send2(0x35,0x83);
}

/********** UART frame parser + roof probe **********/
static bool got_roof_once = false;
static uint32_t last_probe_ms = 0;

static void process_uart(){
  static std::vector<uint8_t> frame;
  while (HRV.available()) {
    uint8_t b = HRV.read();
    if (frame.empty()) { if (b==0x7E) frame.push_back(b); continue; }
    frame.push_back(b);
    if (b==0x7E && frame.size()>=7){
      size_t cks = frame.size()-2; int sum=0; for (size_t i=1;i<cks;++i) sum -= frame[i];
      bool ok = ((uint8_t)(sum & 0xFF) == frame[cks]);
      if (ok){
        last_rx_any_ms = millis();
        uint8_t t = frame[1];
        if (t==0x31 && frame.size()==10){
          uint16_t raw = (frame[2]<<8) | frame[3];
          float tmpc = raw * 0.0625f;
          if (!sht_ok && tmpc>=0 && tmpc<=45) {
            last_house_temp = roundf(tmpc*10)/10.0f; ts_house = millis();
            if (ui_House_Temp_Value) ui_set_label_text(ui_House_Temp_Value, String(last_house_temp,1));
            if (mqtt.connected()) mqtt.publish(T_HOUSE_TEMP, String(last_house_temp,1).c_str(), true);
          }
          int fan = frame[4];
          if (fan>=0 && fan<=100){ last_fan_speed=fan; ts_fan=millis(); }
        } else if (t==0x30 && frame.size()==7){
          uint16_t raw = (frame[2]<<8) | frame[3];
          float tmpc = raw * 0.0625f;
          if (tmpc>=0 && tmpc<=45){
            last_roof_temp = roundf(tmpc*10)/10.0f; ts_roof = millis();
            got_roof_once = true;
            if (ui_Roof_Temp_Value) ui_set_label_text(ui_Roof_Temp_Value, String(last_roof_temp,1));
          }
        }
      }
      frame.clear();
    }
    if (frame.size()>24) frame.clear();
  }
}

/***********************
 *  Auto control task
 ***********************/
static void auto_control_task(bool force_send=false){
  if (power_off || boost_active) return;

  // Manual override active?
  if (!auto_mode){
    if ((int32_t)(millis() - manual_hold_until_ms) < 0) return; // still manual
    auto_mode = true; // timeout -> back to auto
  }

  // AUTO fallback when temps unknown -> 30%
  if (last_house_temp==255 || last_roof_temp==255){
    int p = UNKNOWN_FAN_FALLBACK; // 30%
    if (!force_send && abs(p - target_percent) < 2 && abs(p - last_auto_percent) < 2) return;
    last_auto_percent = p;
    target_percent = p;
    if (p > 0) last_nonzero_percent = p;
    send_fan_now(p);
    return;
  }

  float err = setpoint_c - last_house_temp;          // + => want warmer
  float abs_err = fabsf(err);
  float roof_delta = last_roof_temp - last_house_temp; // + => roof warmer

  int p = MIN_TRICKLE;

  if (abs_err <= SP_TOL_C) {
    p = MIN_TRICKLE;
  } else {
    bool heating = (err > 0);
    bool helpful = (heating && roof_delta >  ROOF_HELP_DEADBAND) ||
                   (!heating && roof_delta < -ROOF_HELP_DEADBAND);

    if (!helpful) {
      p = MIN_TRICKLE;
    } else {
      float mag = fabsf(roof_delta) - ROOF_HELP_DEADBAND;
      float raw = MIN_HELP_SPEED + GAIN_PCT_PER_C * mag;
      if (abs_err < 1.0f) raw = fminf(raw, (float)NEAR_SP_MAX);
      p = constrain((int)roundf(raw), MIN_TRICKLE, 100);
    }
  }

  if (!force_send && abs(p - target_percent) < 2 && abs(p - last_auto_percent) < 2) return;

  last_auto_percent = p;
  target_percent = p;
  if (p > 0) last_nonzero_percent = p;
  send_fan_now(p);
}


/***********************
 *  MQTT
 ***********************/
static void mqtt_callback(char* topic, byte* payload, unsigned int len){
  String t(topic); String x; x.reserve(len); for (unsigned int i=0;i<len;i++) x+=(char)payload[i];

  if (t==T_FILTER_DAYS){
    int d = x.toInt(); hrv_days_remaining = d;
    int life = (int)round(d*100.0/730.0); set_filter_text_by_pct(life);
    if (mqtt.connected()){
      mqtt.publish(T_FILTER_LIFE, String(life).c_str(), true);
      mqtt.publish(T_FILTER_NEEDED, (life<=0)?"ON":"OFF", true);
    }
  } else if (t==T_FILTER_LIFE){
    int lf = x.toInt(); set_filter_text_by_pct(lf);
  } else if (t==T_FAN_PERCENT){
    if (power_off || boost_active) return;  // ignore remote while disabled
    int p = x.toInt(); p = constrain(p,0,100);
    if (p == target_percent) return;        // ignore echoes
    target_percent=p; if (p>0) last_nonzero_percent=p; send_fan_now(p);
    // mark manual override
    auto_mode = false;
    manual_hold_until_ms = millis() + MANUAL_HOLD_MS;
  } else if (t==T_SETPOINT_CMD){
    float v = x.toFloat();
    // apply without beep; this also publishes state back
    // and puts AUTO in charge (unless power/boost)
    v = constrain(v, 5.0f, 35.0f);
    if (fabsf(v - setpoint_c) > 0.01f) {
      setpoint_c = v;
      prefs.putFloat(KEY_SETPOINT, setpoint_c);
      reflect_setpoint_ui();
      if (mqtt.connected()) mqtt.publish(T_SETPOINT_STATE, String(setpoint_c,1).c_str(), true);
      if (!power_off && !boost_active){
        auto_mode = true;
        manual_hold_until_ms = 0;
        auto_control_task(true);
      }
    }
  }
}

static void mqtt_loop_task(){
  if (!WiFi.isConnected() || ap_mode) { mqtt.disconnect(); return; }
  if (mqtt.connected()) { mqtt.loop(); return; }
  if (millis() < next_mqtt_try_ms) return;
  if (mqtt_host.length()==0) { next_mqtt_try_ms = millis()+5000; return; }

  mqtt.setServer(mqtt_host.c_str(), mqtt_port);
  mqtt.setCallback(mqtt_callback);   // ensure callback set before connect

  if (mqtt.connect(MQTT_CLIENTID, mqtt_user.c_str(), mqtt_pass.c_str())){
    mqtt.subscribe(T_FILTER_DAYS,1);
    mqtt.subscribe(T_FILTER_LIFE,1);
    mqtt.subscribe(T_FAN_PERCENT,1);
    mqtt.subscribe(T_SETPOINT_CMD,1);             // listen for retained setpoint

    // publish retained state
    mqtt.publish(T_SETPOINT_STATE, String(setpoint_c,1).c_str(), true);
    if (last_house_temp!=255) mqtt.publish(T_HOUSE_TEMP, String(last_house_temp,1).c_str(), true);
    if (!isnan(last_house_hum)) mqtt.publish(T_HOUSE_HUM,  String(last_house_hum,1).c_str(), true);
  }
  next_mqtt_try_ms = millis() + 3000;
}


/***********************
 *  ACTIONS
 ***********************/
static void apply_power_off(){
  power_off = true;
  if (boost_active) { boost_active=false; }
  set_boost_button_enabled(false);          // only Power remains usable
  set_slider_enabled(false);
  set_setpoint_enabled(false);
  target_percent = 0;
  send_fan_now(0);
}

static void apply_power_on(){
  power_off = false;
  set_boost_button_enabled(true);
  set_slider_enabled(true);
  set_setpoint_enabled(true);

  // Immediately give control back to AUTO using the saved/retained setpoint
  auto_mode = true;
  manual_hold_until_ms = 0;
  auto_control_task(true);   // if temps unknown => 30%
}

static void start_burnt_toast(){
  if (power_off || boost_active) return;
  boost_active=true; boost_end_ms=millis()+boost_duration_ms;
  if (mqtt.connected()) mqtt.publish(T_BOOST_ACTIVE,"ON",true);

  set_slider_enabled(false);
  set_setpoint_enabled(false);
  auto_mode = false; // Toast overrides auto

  // Per request: Burnt Toast = 100%
  target_percent = 100;
  last_nonzero_percent = 100;
  send_fan_now(100);

  if (ui_Burnt_Toast) lv_obj_add_state(ui_Burnt_Toast, LV_STATE_CHECKED);
}

static void stop_burnt_toast(){
  if (!boost_active) return;
  boost_active=false; if (mqtt.connected()) mqtt.publish(T_BOOST_ACTIVE,"OFF",true);

  if (!power_off){
    set_slider_enabled(true);
    set_setpoint_enabled(true);

    // After Toast, hold manual 30% for the window to avoid fighting AUTO
    auto_mode = false;
    manual_hold_until_ms = millis() + MANUAL_HOLD_MS;
    target_percent = 30;
    last_nonzero_percent = 30;
    send_fan_now(30);
  }

  if (ui_Burnt_Toast) lv_obj_clear_state(ui_Burnt_Toast, LV_STATE_CHECKED);
}

/***********************
 *  Setpoint button helpers (beep only here)
 ***********************/
static const float SETPOINT_STEP = 0.5f;   // change to 1.0f for whole degrees

static inline void save_setpoint_to_nvs(float v){
  prefs.putFloat(KEY_SETPOINT, v);
}

static inline void apply_new_setpoint(float v, bool do_beep){
  v = constrain(v, 5.0f, 35.0f);
  if (fabsf(v - setpoint_c) < 0.01f) return;   // no change
  setpoint_c = v;
  reflect_setpoint_ui();
  save_setpoint_to_nvs(setpoint_c);
  if (mqtt.connected()) mqtt.publish(T_SETPOINT_STATE, String(setpoint_c,1).c_str(), true);
  if (do_beep) beep_now();

  // Setpoint press puts AUTO in charge immediately (unless power/boost)
  if (!power_off && !boost_active){
    auto_mode = true;
    manual_hold_until_ms = 0;
    auto_control_task(true);   // unknown temps => 30% now
  }
}

#ifdef UI_HAS_SETPOINT
static void on_setpoint_up_cb(lv_event_t*)   { apply_new_setpoint(setpoint_c + SETPOINT_STEP, true); }
static void on_setpoint_down_cb(lv_event_t*) { apply_new_setpoint(setpoint_c - SETPOINT_STEP, true); }
#endif

/***********************
 *  UI callbacks
 ***********************/
static void on_power_cb(lv_event_t*){
  if (!power_off) apply_power_off(); else apply_power_on();
}
static void on_burnt_toast_cb(lv_event_t*){
  if (power_off) return;
  if (boost_active) stop_burnt_toast(); else start_burnt_toast();
}
static void on_fan_slider_cb(lv_event_t* e){
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED){ slider_drag = true; return; }
  if (code != LV_EVENT_RELEASED) return;
  slider_drag = false;
  if (boost_active || ui_prog || power_off) return;

  int p = lv_slider_get_value((lv_obj_t*)lv_event_get_target(e));
  p = constrain(p, 0, 100);
  target_percent = p;
  if (p > 0) last_nonzero_percent = p;
  send_fan_now(p);
  ui_reflect_block_until = millis() + 350;

  // manual override sticky window (takes over from AUTO)
  auto_mode = false;
  manual_hold_until_ms = millis() + MANUAL_HOLD_MS;
}

/***********************
 *  HTTP handlers
 ***********************/
static void handle_root(){ web.send_P(200,"text/html",INDEX_HTML); }
static void handle_status() {
  struct tm tmNow; char hhmm[6]="--:--";
  if (getLocalTime(&tmNow)) strftime(hhmm,sizeof(hhmm),"%H:%M",&tmNow);
  int lifePct = (int)round(hrv_days_remaining * 100.0 / 730.0);

  IPAddress ip = WiFi.isConnected() ? WiFi.localIP() : WiFi.softAPIP();
  String wifiMode = WiFi.isConnected() ? "STA" : (ap_mode ? "AP" : "DISCONNECTED");

  String json = "{";
  json += "\"ip\":\""+ ip.toString() +"\",";
  json += "\"rssi\":"+ String(WiFi.isConnected()?WiFi.RSSI():0) + ",";
  json += "\"wifi_mode\":\""+ wifiMode +"\",";
  json += "\"time\":\""+ String(hhmm) +"\",";

  json += "\"house\":";
  json += (last_house_temp==255) ? "null," : String(last_house_temp,1)+",";

  json += "\"hum\":";
  json += (isnan(last_house_hum)) ? "null," : String(last_house_hum,1)+",";

  json += "\"roof\":";
  json += (last_roof_temp==255) ? "null," : String(last_roof_temp,1)+",";

  json += "\"power_off\":" + String(power_off?"true":"false") + ",";
  json += "\"boost\":"     + String(boost_active?"true":"false") + ",";
  json += "\"slider_disabled\":" + String((power_off||boost_active)?"true":"false") + ",";

  json += "\"auto\":" + String(auto_mode?"true":"false") + ",";
  json += "\"setpoint\":" + String(setpoint_c,1) + ",";

  json += "\"fan_target\":"+ String(target_percent) + ",";
  json += "\"fan_actual\":"+ String(last_fan_speed==255?target_percent:last_fan_speed) + ",";

  json += "\"filter_pct\":"+ String(lifePct) + ",";
  json += "\"filter_text\":\""+ String(lifePct<=0?"REPLACE":"OK") + "\",";

  json += "\"mode\":\""+ current_mode() +"\"";
  json += "}";
  web.send(200,"application/json",json);
}
static void handle_setfan() {
  if (boost_active || power_off) { web.send(200,"application/json","{\"ok\":false}"); return; }
  int p = web.hasArg("val") ? web.arg("val").toInt() : target_percent;
  p = constrain(p,0,100);
  target_percent = p; if (p>0) last_nonzero_percent = p;
  send_fan_now(p);
  // manual override sticky window
  auto_mode = false;
  manual_hold_until_ms = millis() + MANUAL_HOLD_MS;
  web.send(200,"application/json","{\"ok\":true}");
}
static void handle_power() {
  if (!power_off) apply_power_off(); else apply_power_on();
  web.send(200,"application/json","{\"ok\":true}");
}
static void handle_boost() {
  if (power_off){ web.send(200,"application/json","{\"ok\":false}"); return; }
  if (boost_active) stop_burnt_toast(); else start_burnt_toast();
  web.send(200,"application/json","{\"ok\":true}");
}

// setpoint + auto endpoints
static void handle_setpoint(){
  if (!web.hasArg("c")) { web.send(400,"application/json","{\"ok\":false,\"err\":\"missing c\"}"); return; }
  float v = web.arg("c").toFloat();
  v = constrain(v, 5.0f, 35.0f);
  setpoint_c = v;

  // persist to NVS
  prefs.putFloat(KEY_SETPOINT, setpoint_c);
  if (mqtt.connected()) mqtt.publish(T_SETPOINT_STATE, String(setpoint_c,1).c_str(), true);

  reflect_setpoint_ui();
  // Web setpoint: put AUTO back in charge immediately
  if (!power_off && !boost_active){
    auto_mode = true;
    manual_hold_until_ms = 0;
    auto_control_task(true);   // if temps unknown => 30%
  }
  web.send(200,"application/json","{\"ok\":true}");
}
static void handle_auto(){
  if (web.hasArg("mode")){
    String m = web.arg("mode");
    if (m == "manual"){
      auto_mode = false;
      manual_hold_until_ms = millis() + MANUAL_HOLD_MS;
    } else {
      auto_mode = true;
      manual_hold_until_ms = 0;
      if (!power_off && !boost_active) auto_control_task(true);
    }
  }
  web.send(200,"application/json","{\"ok\":true}");
}

static void handle_mqtt_cfg(){
  bool changed = false;

  if (web.hasArg("host")) {
    String v = web.arg("host");
    v.trim();
    if (v.length()) { mqtt_host = v; prefs.putString(KEY_MQTT_HOST, mqtt_host); changed = true; }
  }
  if (web.hasArg("port")) {
    int v = web.arg("port").toInt();
    if (v >= 1 && v <= 65535) { mqtt_port = (uint16_t)v; prefs.putUInt(KEY_MQTT_PORT, mqtt_port); changed = true; }
  }
  if (web.hasArg("user")) {
    // allow empty user if brokers don’t require auth
    mqtt_user = web.arg("user");
    prefs.putString(KEY_MQTT_USER, mqtt_user);
    changed = true;
  }
  if (web.hasArg("pass")) {
    mqtt_pass = web.arg("pass");
    prefs.putString(KEY_MQTT_PASS, mqtt_pass);
    changed = true;
  }

  if (changed){
    mqtt.disconnect();
    next_mqtt_try_ms = 0;         // reconnect ASAP
    web.send(200,"application/json","{\"ok\":true}");
    return;
  }

  // GET current (mask password in UI if you prefer)
  String json="{";
  json+="\"host\":\""+mqtt_host+"\",";
  json+="\"port\":"+String(mqtt_port)+",";
  json+="\"user\":\""+mqtt_user+"\",";
  json+="\"pass\":\""+mqtt_pass+"\"";
  json+="}";
  web.send(200,"application/json",json);
}


/***********************
 *  CLOCK UI
 ***********************/
static void update_clock_ui_once(){
  if (!ui_Time_Value) return; struct tm tmNow; if (!getLocalTime(&tmNow)) return;
  char hhmm[6]; strftime(hhmm,sizeof(hhmm),"%H:%M",&tmNow); ui_set_label_text(ui_Time_Value, String(hhmm));
}

/***********************
 *  SHT31 poller
 ***********************/
static void poll_sht31(){
  if (!sht_ok) return;
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();

  if (!isnan(t)){
    last_house_temp = roundf(t*10)/10.0f; ts_house = millis();
    if (ui_House_Temp_Value) ui_set_label_text(ui_House_Temp_Value, String(last_house_temp,1));
    if (mqtt.connected())    mqtt.publish(T_HOUSE_TEMP, String(last_house_temp,1).c_str(), true);
  }
  if (!isnan(h)){
    last_house_hum = roundf(h*10)/10.0f;
    if (mqtt.connected())    mqtt.publish(T_HOUSE_HUM, String(last_house_hum,1).c_str(), true);
  }
}

/***********************
 *  WIFI (non-blocking; safe start point for WebServer/mDNS)
 ***********************/
static void start_web_if_needed(){
  if (web_started) return;
  web.on("/", handle_root);
  web.on("/status", handle_status);
  web.on("/api/setfan", handle_setfan);
  web.on("/api/power",  handle_power);
  web.on("/api/boost",  handle_boost);
  web.on("/api/setpoint", handle_setpoint);
  web.on("/api/auto",     handle_auto);
  web.on("/mqtt",         handle_mqtt_cfg);   // <--- MQTT settings endpoint
  web.begin();
  web_started = true;
}
static void wifi_loop_task(){
  if (ap_mode) {
    if (!web_started) start_web_if_needed();
    return;
  }
  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED){
    if (!mdns_started){
      if (MDNS.begin("hrv-keypad")) MDNS.addService("http","tcp",80);
      mdns_started = true;
    }
    if (!web_started) start_web_if_needed();
    return;
  }
  if (wifi_begin_ms == 0){
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);    // non-blocking
    wifi_begin_ms = millis();
    return;
  }
  if (millis() - wifi_begin_ms > 8000){
    WiFi.disconnect(true, true);
    delay(100);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    ap_mode = true;
    if (!web_started) start_web_if_needed();
  }
}

/***********************
 *  SETUP / LOOP
 ***********************/
void setup(){
  // Screen + LVGL first
  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);

  tft.init(); tft.setRotation(0); tft.fillScreen(TFT_BLACK);
  touchSPI.begin(PIN_TOUCH_SCLK, PIN_TOUCH_MISO, PIN_TOUCH_MOSI, PIN_TOUCH_CS);
  ts.begin(touchSPI); ts.setRotation(TS_ROT);

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_W * 40);
  static lv_disp_drv_t disp_drv; lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res=SCREEN_W; disp_drv.ver_res=SCREEN_H; disp_drv.flush_cb=my_disp_flush; disp_drv.draw_buf=&draw_buf; lv_disp_drv_register(&disp_drv);
  static lv_indev_drv_t indev_drv; lv_indev_drv_init(&indev_drv);
  indev_drv.type=LV_INDEV_TYPE_POINTER; indev_drv.read_cb=my_touch_read; lv_indev_drv_register(&indev_drv);

  // buzzer
  buzzer_init();

  // UI
  ui_init();

  if (ui_Fan_Speed_Slider) {
    lv_slider_set_range(ui_Fan_Speed_Slider, 0, 100);
    lv_obj_set_style_anim_time(ui_Fan_Speed_Slider, 0, LV_PART_MAIN);
    lv_obj_set_style_anim_time(ui_Fan_Speed_Slider, 0, LV_PART_KNOB);
    lv_obj_add_event_cb(ui_Fan_Speed_Slider, on_fan_slider_cb, LV_EVENT_PRESSED,  NULL);
    lv_obj_add_event_cb(ui_Fan_Speed_Slider, on_fan_slider_cb, LV_EVENT_RELEASED, NULL);
  }
  if (ui_Power)       lv_obj_add_event_cb(ui_Power, on_power_cb, LV_EVENT_CLICKED, NULL);
  if (ui_Burnt_Toast) lv_obj_add_event_cb(ui_Burnt_Toast, on_burnt_toast_cb, LV_EVENT_CLICKED, NULL);

#ifdef UI_HAS_SETPOINT
  // Hook Setpoint Up/Down buttons (rename IDs if yours differ)
  if (ui_Setpoint_Up)    lv_obj_add_event_cb(ui_Setpoint_Up,    on_setpoint_up_cb,   LV_EVENT_CLICKED, NULL);
  if (ui_Setpoint_Down)  lv_obj_add_event_cb(ui_Setpoint_Down,  on_setpoint_down_cb, LV_EVENT_CLICKED, NULL);
#endif

  // I2C + SHT31
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  sht_ok = sht31.begin(0x44); if (!sht_ok) sht_ok = sht31.begin(0x45);

  // Time
  configTzTime(TIMEZONE, NTP1, NTP2, NTP3);

  // Persistence: load last setpoint + MQTT config
  prefs.begin(NVS_NS, false);
  setpoint_c = prefs.getFloat(KEY_SETPOINT, 18.0f);   // default 18C if not saved yet
  mqtt_host  = prefs.getString(KEY_MQTT_HOST, MQTT_HOST);
  mqtt_port  = (uint16_t)prefs.getUInt(KEY_MQTT_PORT, MQTT_PORT);
  mqtt_user  = prefs.getString(KEY_MQTT_USER, MQTT_USER);
  mqtt_pass  = prefs.getString(KEY_MQTT_PASS, MQTT_PASS);
  reflect_setpoint_ui();

  // HRV UART + handshake
  HRV.begin(1200, SERIAL_8N1, HRV_RX_PIN, HRV_TX_PIN);
  boot_handshake();

  // Start in AUTO using saved setpoint; if temps unknown, fallback drives 30%
  auto_mode = true;
  manual_hold_until_ms = 0;
  last_auto_percent = -1;
  auto_control_task(true);

  // Initial UI/state indicators
  reflect_fan_percent_on_ui(target_percent);
  set_filter_text_by_pct((int)round(hrv_days_remaining*100.0/730.0));
}

void loop(){
  lv_timer_handler(); delay(5);

  // service the software buzzer
  beep_service();

  wifi_loop_task();     // brings up STA or AP, then web/mDNS
  mqtt_loop_task();     // only runs when STA connected
  if (web_started) web.handleClient();

  process_uart();

  // probe for roof until we see a 0x30 frame once
  if (!got_roof_once && millis() - last_probe_ms > 2000) {
    last_probe_ms = millis();
    send2(0x33, 0xC3); delay(50); send2(0x43, 0xC3);
  }

  if (sht_ok && millis()-last_sht_ms >= SHT_PERIOD_MS){ last_sht_ms = millis(); poll_sht31(); }

  // keepalive to HRV
  if (millis()-last_sent_ms > 5000) send_fan_now(target_percent);

  // Burnt Toast timeout
  if (boost_active && (int32_t)(millis()-boost_end_ms)>=0) stop_burnt_toast();

  // Sensor timeouts → show 0 on UI
  if (!sht_ok && millis()-ts_house>sensor_timeout_ms && ui_House_Temp_Value) ui_set_label_text(ui_House_Temp_Value,"0");
  if (millis()-ts_roof>sensor_timeout_ms && ui_Roof_Temp_Value) ui_set_label_text(ui_Roof_Temp_Value,"0");

  // Auto control from setpoint (respects power/boost/manual)
  auto_control_task(false);

  // Clock
  if (millis()-last_clock_ms>=1000){ last_clock_ms=millis(); update_clock_ui_once(); }
}

static_assert(sizeof(lv_color_t)==2, "LV_COLOR_DEPTH must be 16");
