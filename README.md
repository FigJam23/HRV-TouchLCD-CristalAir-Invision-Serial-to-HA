# HRV → Home Assistant

A simple ESP8266/D1 Mini sketch that reads TTL-serial data from your HRV control panel and publishes it to Home Assistant via MQTT. Pulls house & roof temperatures, fan speed and control-panel setpoint; plans for sending control commands in a future update.

---

## 📦 Hardware

- ESP8266 (D1 Mini shown; any 3.3 V ESP8266 will work)  
- TTL logic-level shifter (5 V → 3.3 V)   
- 1 cap
- Optional: buck converter if you switch to an external 5 V supply





<div style="
  display: flex;
  justify-content: center;
  align-items: flex-start;
  gap: 0.5em;
">
  <img
    style="width: 30%; height: auto;"
    alt="first diagram"
    src="https://github.com/user-attachments/assets/18cdaeea-f2a3-4aa9-b0f8-0c1edc792d2e"
  />
  <img
    style="width: 30%; height: auto;"
    alt="second diagram"
    src="https://github.com/user-attachments/assets/ccee0cb8-b140-4ca7-a427-a638f3edf10d"
  />
  <img
    style="
      width: 30%;
      height: auto;
      transform: rotate(90deg);
      transform-origin: center center;
    "
    alt="third diagram (rotated)"
    src="https://github.com/user-attachments/assets/1ade8a6f-7d6c-4dc5-b432-deb18a56c4c8"
  />
</div>









```
Components

ESP8266 (Wemos D1 Mini)

HRV Keypad

Logic Level Shifter (TX/RX)

Wiring Connections

5V → Positive (5V) [Blue]

GND → Negative (GND) [Black]

RX → LV1 ↔ HV1 → TX [Green]

TX → LV2 ↔ HV2 → RX [White]

Notes

Use logic level shifter for safe 3.3V-5V communication.

Verify RX/TX correctly connected.
```
---

## 🚀 Features

- **Telemetry**  
  - House temperature  
  - Roof temperature  
  - Control-panel setpoint  
  - Fan speed  
- **MQTT topics**  
  ```cpp
  #define HASSIOHRVSTATUS      "hassio/hrv/status"      // Alive heartbeat
  #define HASSIOHRVSUBHOUSE    "hassio/hrv/housetemp"   // House temp
  #define HASSIOHRVSUBROOF     "hassio/hrv/rooftemp"    // Roof temp
  #define HASSIOHRVSUBCONTROL  "hassio/hrv/controltemp" // Control-panel setpoint
  #define HASSIOHRVSUBFANSPEED "hassio/hrv/fanspeed"    // Fan speed
  #define HASSIOHRVRAW         "hassio/hrv/raw"         // Raw packet hex


---
```
⚡ ESP8266 Sketch

#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>

// HRV constants
#define MSGSTARTSTOP 0x7E
#define HRVROOF      0x30
#define HRVHOUSE     0x31

// MQTT topics
#define HASSIOHRVSTATUS      "hassio/hrv/status"
#define HASSIOHRVSUBHOUSE    "hassio/hrv/housetemp"
#define HASSIOHRVSUBROOF     "hassio/hrv/rooftemp"
#define HASSIOHRVSUBCONTROL  "hassio/hrv/controltemp"
#define HASSIOHRVSUBFANSPEED "hassio/hrv/fanspeed"
#define HASSIOHRVRAW         "hassio/hrv/raw"
// Manual‐command topic
#define HASSIOHRVMANUAL      "hassio/hrv/manual"

// Forward declarations
void    myDelay(int ms);
void    startWIFI();
String  decToHex(byte val, byte width);
unsigned int hexToDec(String hex);
void    SendMQTTMessage();
void    mqttCallback(char* topic, byte* payload, unsigned int length);

// Wi-Fi credentials
const char* ssid     = "Reaver";
const char* password = "Figjam";

// MQTT broker
IPAddress MQTT_SERVER(192, 168, 1, 44);
const char* mqttUser     = "mqtt";
const char* mqttPassword = "Just";

// TTL HRV serial on D2=RX, D1=TX
SoftwareSerial hrvSerial(D2, D1);

// Packet parsing & state
bool   bStarted        = false, bEnded = false;
byte   inData[10], bIndex = 0, bChecksum = 0;
float  fHRVTemp, fHRVLastRoof = 255, fHRVLastHouse = 255;
int    iHRVControlTemp, iHRVFanSpeed, iHRVLastControl = 255, iHRVLastFanSpeed = 255;
int    iTotalDelay     = 0;
char   eTempLoc;

WiFiClient   wifiClient;
PubSubClient mqttClient(MQTT_SERVER, 1883, wifiClient);
String       clientId;
char         message_buff[16];
String       pubString;

// ─────────────────────────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  hrvSerial.begin(1200);
  myDelay(10);

  // connect Wi-Fi & prep MQTT
  startWIFI();
  clientId = "hrv-" + WiFi.macAddress();
  mqttClient.setServer(MQTT_SERVER, 1883);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setKeepAlive(120);

  // subscribe to manual hex commands
  mqttClient.subscribe(HASSIOHRVMANUAL);

  // reset parser state
  bIndex     = 0;
  bChecksum  = 0;
  iTotalDelay= 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// MAIN LOOP
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  // MQTT reconnect
  if (!mqttClient.connected()) {
    startWIFI();
    while (!mqttClient.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      delay(2000);
    }
    mqttClient.subscribe(HASSIOHRVMANUAL);
  }
  mqttClient.loop();

  // serial-data LED
  digitalWrite(LED_BUILTIN, hrvSerial.available() ? HIGH : LOW);

  // Read & buffer HRV bytes
  while (hrvSerial.available() > 0) {
    int c = hrvSerial.read();
    if (c == MSGSTARTSTOP || bIndex > 8) {
      if (bIndex == 0)        bStarted = true;
      else { bChecksum = bIndex - 1; bEnded = true; break; }
    }
    if (bStarted) inData[bIndex++] = c;
    myDelay(1);
  }

  // Validate checksum
  if (bStarted && bEnded && bChecksum > 0) {
    int sum = 0;
    for (int i = 1; i < bChecksum; i++) sum -= inData[i];
    byte calc = byte(sum % 0x100);
    if (decToHex(calc,2) != decToHex(inData[bChecksum],2) || bIndex < 6) {
      bStarted = bEnded = false;
      bIndex = 0;
      hrvSerial.flush();
    }
    bChecksum = 0;
  }

  // Process complete packet
  if (bStarted && bEnded && bIndex > 5) {
    String sHex1, sHex2;
    for (int i = 1; i <= bIndex; i++) {
      if      (i == 1)                           eTempLoc        = inData[i];
      else if (i == 2)                           sHex1           = decToHex(inData[i],2);
      else if (i == 3)                           sHex2           = decToHex(inData[i],2);
      else if (eTempLoc == HRVHOUSE && i == 4)   iHRVFanSpeed    = inData[i];
      else if (eTempLoc == HRVHOUSE && i == 5)   iHRVControlTemp = inData[i];
    }
    fHRVTemp = hexToDec(sHex1 + sHex2) * 0.0625;

    // raw log
    {
      String raw;
      for (int i = 0; i < bIndex; i++) {
        if (inData[i] < 0x10) raw += "0";
        raw += String(inData[i], HEX) + " ";
      }
      raw.toUpperCase();
      mqttClient.publish(HASSIOHRVRAW, raw.c_str());
    }

    SendMQTTMessage();

    bStarted = bEnded = false;
    bIndex = 0;
  }
  else {
    myDelay(2000);
  }

  // still-alive ping
  if (WiFi.status()==WL_CONNECTED && mqttClient.connected() && iTotalDelay >= 30000) {
    mqttClient.publish(HASSIOHRVSTATUS, "1");
    iTotalDelay = 0;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// MQTT CALLBACK — write manual hex back to HRV
// ─────────────────────────────────────────────────────────────────────────────
void mqttCallback(char* topic, byte* payload, unsigned int len) {
  if (String(topic) != HASSIOHRVMANUAL) return;
  String msg;
  for (unsigned int i=0; i<len; i++) msg += char(payload[i]);
  msg.trim();
  // parse "AA BB CC ..." into bytes
  int idx = 0;
  while (idx < msg.length()-1) {
    String hexByte = msg.substring(idx, idx+2);
    byte b = strtoul(hexByte.c_str(), nullptr, 16);
    hrvSerial.write(b);
    idx += 2;
    // skip spaces
    while (idx<msg.length() && !isHexadecimalDigit(msg[idx])) idx++;
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// HELPERS
// ─────────────────────────────────────────────────────────────────────────────
String decToHex(byte val, byte width) {
  String s = String(val, HEX);
  while (s.length() < width) s = "0" + s;
  return s;
}
unsigned int hexToDec(String hex) {
  unsigned int v = 0;
  for (char c : hex) {
    int d = isdigit(c) ? c - '0'
          : isupper(c) ? c - 'A' + 10
          : islower(c) ? c - 'a' + 10
          : 0;
    v = v*16 + d;
  }
  return v;
}
void SendMQTTMessage() {
  if (WiFi.status() != WL_CONNECTED || !mqttClient.connected()) return;

  // publish temps
  int tmp = int(fHRVTemp*2 + 0.5);
  fHRVTemp = tmp/2.0;
  pubString = String(fHRVTemp);
  pubString.toCharArray(message_buff, pubString.length()+1);
  if (eTempLoc==HRVHOUSE && fHRVTemp!=fHRVLastHouse) {
    fHRVLastHouse = fHRVTemp;
    mqttClient.publish(HASSIOHRVSUBHOUSE, message_buff);
  } else if (eTempLoc==HRVROOF && fHRVTemp!=fHRVLastRoof) {
    fHRVLastRoof = fHRVTemp;
    mqttClient.publish(HASSIOHRVSUBROOF, message_buff);
  }

  // publish control setpoint
  if (iHRVControlTemp!=iHRVLastControl || iHRVLastFanSpeed==255) {
    pubString = String(iHRVControlTemp);
    pubString.toCharArray(message_buff, pubString.length()+1);
    iHRVLastControl = iHRVControlTemp;
    mqttClient.publish(HASSIOHRVSUBCONTROL, message_buff);
  }

  // publish fan speed
  if (iHRVFanSpeed!=iHRVLastFanSpeed ||
      iHRVControlTemp!=iHRVLastControl ||
      (eTempLoc==HRVHOUSE && fHRVTemp!=fHRVLastHouse) ||
      (eTempLoc==HRVROOF  && fHRVTemp!=fHRVLastRoof))
  {
    iHRVLastFanSpeed = iHRVFanSpeed;
    if      (iHRVFanSpeed==0)   pubString = "Off";
    else if (iHRVFanSpeed==5)   pubString = "Idle";
    else if (iHRVFanSpeed==100) pubString = "Full";
    else                        pubString = String(iHRVFanSpeed) + "%";
    pubString.toCharArray(message_buff, pubString.length()+1);
    mqttClient.publish(HASSIOHRVSUBFANSPEED, message_buff);
  }

  // flash LED
  digitalWrite(LED_BUILTIN, LOW);  myDelay(50);
  digitalWrite(LED_BUILTIN, HIGH); myDelay(1000);
}
void myDelay(int ms) {
  for (int i = 1; i <= ms; i++) {
    delay(1);
    if (i % 100 == 0) {
      ESP.wdtFeed();
      mqttClient.loop();
      yield();
    }
  }
  iTotalDelay += ms;
}
void startWIFI() {
  if (WiFi.status()==WL_CONNECTED) return;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status()!=WL_CONNECTED) {
    delay(2000);
    if (++tries > 450) ESP.reset();
  }
  myDelay(1500);
}
```


MIT © Mike Figjam



