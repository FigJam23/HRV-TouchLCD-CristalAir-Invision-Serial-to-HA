```
/***********************
 *  ESP32 RXTX to HRV Bus Comms
 ***********************/
#define HRV_RX_PIN 16
#define HRV_TX_PIN 17

/***********************
 *  SHT31 on I²C
 ***********************/
static const int PIN_I2C_SDA = 27;
static const int PIN_I2C_SCL = 22;

/***********************
 * Default AP Logins
 ***********************/

static const char* AP_SSID       = "HRV-Keypad";
static const char* AP_PASS       = "12345678";
```
# Touch Screen Pins For Direct wiring to custom Touch Screen / Will work With or without a Screen!!!!!!!! 
```
TFT_eSPI tft;

// XPT2046 on HSPI (CYD)
static const int PIN_TOUCH_CS   = 33;
static const int PIN_TOUCH_IRQ  = 36;
static const int PIN_TOUCH_MOSI = 32;
static const int PIN_TOUCH_MISO = 39;
static const int PIN_TOUCH_SCLK = 25;
static const int PIN_TFT_BL     = 21;

// --- BUZZER: 2-pin header on back of screen ---
static const int BUZZER_PIN = 26;
```
