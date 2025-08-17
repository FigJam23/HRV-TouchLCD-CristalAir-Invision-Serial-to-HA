https://youtube.com/shorts/8ISQGKKpurk?si=fGekwMP1LDX2R__V

```
static const char* AP_SSID       = "HRV-Keypad";
static const char* AP_PASS       = "12345678";  // 8+ chars required by ESP

// MQTT topics
static const char* T_FILTER_DAYS     = "hassio/hrv/filter_days_remaining/state";
static const char* T_FILTER_LIFE     = "hassio/hrv/filter_life/state";
static const char* T_FAN_PERCENT     = "hassio/hrv/fan_percent/state";
static const char* T_BOOST_ACTIVE    = "hassio/hrv/boost_active/state";
static const char* T_FILTER_NEEDED   = "hassio/hrv/filter_replacement_needed";
static const char* T_HOUSE_TEMP      = "hassio/hrv/house_temp/state";
static const char* T_HOUSE_HUM       = "hassio/hrv/house_humidity/state";
static const char* T_SETPOINT_STATE  = "hassio/hrv/setpoint/state"; // retained
static const char* T_SETPOINT_CMD    = "hassio/hrv/setpoint/set";   // retained command (optional from HA)
// Burnt Toast countdown
static const char* T_BOOST_REMAIN_S  = "hassio/hrv/boost_remaining_s/state";
static const char* T_BOOST_TOTAL_S   = "hassio/hrv/boost_total_s/state";
```
<table>
  <tr>
    <td><img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/6e0b0809-e17d-45b1-8727-2efd2609b403.png" width="150"></td>
    <td><img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Screenshot_20250811_145851_AliExpress.jpg" width="150"></td>
    <td><img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/f7614291-c459-4cea-b07e-f4bfe5d946c0(4).png" width="150"></td>
  </tr>
  <tr>
    <td><img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/file_0000000060ec61fd84d62c2a60668eef.png" width="150"></td>
    <td><img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/file_0000000086a461f8abbdfd0f251aac8b(1).png" width="150"></td>
    <td><img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/file_00000000ba5061f8b88bcc3f6a8952d6.png" width="150"></td>
  </tr>
  <tr>
    <td><img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/file_00000000bd6c622f9eaf868b7891e35b.png" width="150"></td>
  </tr>
</table>
