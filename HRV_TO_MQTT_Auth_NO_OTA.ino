#include <PubSubClient.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>

// HRV constants
#define MSGSTARTSTOP 0x7E
#define HRVROOF      0x30
#define HRVHOUSE     0x31

// MQTT topics
#define HASSIOHRVSTATUS    "hassio/hrv/status"
#define HASSIOHRVSUBHOUSE  "hassio/hrv/housetemp"
#define HASSIOHRVSUBROOF   "hassio/hrv/rooftemp"
#define HASSIOHRVSUBCONTROL "hassio/hrv/controltemp"
#define HASSIOHRVSUBFANSPEED "hassio/hrv/fanspeed"
#define HASSIOHRVRAW       "hassio/hrv/raw"

// Forward declarations
void myDelay(int ms);
void startWIFI();

// Wi-Fi credentials
const char* ssid     = "Reav";
const char* password = "Figjam";

// MQTT broker
IPAddress MQTT_SERVER(192, 168, 1, 44);
const char* mqttUser     = "mqtt";
const char* mqttPassword = "Jus";

// SoftwareSerial for HRV TTL
SoftwareSerial hrvSerial(D2, D3);

// Globals for packet parsing & state
bool   bStarted        = false, bEnded = false;
byte   inData[10], bIndex = 0, bChecksum = 0;
float  fHRVTemp, fHRVLastRoof = 255, fHRVLastHouse = 255;
int    iHRVControlTemp, iHRVFanSpeed, iHRVLastControl = 255, iHRVLastFanSpeed = 255;
int    iTotalDelay     = 0;
char   eTempLoc;
WiFiClient      wifiClient;
PubSubClient    mqttClient(MQTT_SERVER, 1883, wifiClient);
String          clientId;
char            message_buff[16];
String          pubString;


// ----------------------------------------------------------------------------
// SETUP
// ----------------------------------------------------------------------------
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  hrvSerial.begin(1200);
  Serial.begin(115200);
  myDelay(10);

  // Connect Wi-Fi & prep MQTT
  startWIFI();
  clientId = "hrv-" + WiFi.macAddress();
  mqttClient.setKeepAlive(120);

  // Reset packet state
  bIndex      = 0;
  bChecksum   = 0;
  iTotalDelay = 0;
}

// ----------------------------------------------------------------------------
// LOOP
// ----------------------------------------------------------------------------
void loop() {
  // ─── MQTT reconnect block ───
  if (!mqttClient.connected()) {
    startWIFI();
    Serial.println("Connecting to MQTT Broker with credentials…");
    clientId = "hrv-" + WiFi.macAddress();
    mqttClient.setKeepAlive(120);
    // Force full resend
    iHRVLastFanSpeed = iHRVLastControl = 255;
    fHRVLastRoof     = fHRVLastHouse   = 255;
    while (!mqttClient.connect(clientId.c_str(), mqttUser, mqttPassword)) {
      Serial.printf("MQTT failed, rc=%d — retrying in 2s\n", mqttClient.state());
      // blink LED 10×
      for (int i = 0; i < 10; i++) {
        digitalWrite(LED_BUILTIN, LOW);  myDelay(75);
        digitalWrite(LED_BUILTIN, HIGH); myDelay(75);
      }
      delay(2000);
    }
    Serial.println("MQTT connected!");
  }
  // ────────────────────────────

  // Indicate serial-data presence
  if (hrvSerial.available() == 0) {
    Serial.println("No serial data detected!");
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }

  // Read & buffer HRV bytes
  while (hrvSerial.available() > 0) {
    int inChar = hrvSerial.read();
    if (inChar == MSGSTARTSTOP || bIndex > 8) {
      if (bIndex == 0) {
        bStarted = true;
      } else {
        bChecksum = bIndex - 1;
        bEnded    = true;
        break;
      }
    }
    if (bStarted) inData[bIndex++] = inChar;
    myDelay(1);
  }

  // Validate checksum
  if (bStarted && bEnded && bChecksum > 0) {
    int sum = 0;
    for (int i = 1; i < bChecksum; i++) sum -= inData[i];
    byte calc = (byte)(sum % 0x100);
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
      if (i==1)                eTempLoc = inData[i];
      if (i==2)                sHex1    = decToHex(inData[i],2);
      if (i==3)                sHex2    = decToHex(inData[i],2);
      if (eTempLoc==HRVHOUSE && i==4) iHRVLastFanSpeed = inData[i];
      if (eTempLoc==HRVHOUSE && i==5) iHRVControlTemp  = inData[i];
    }
    fHRVTemp = hexToDec(sHex1 + sHex2)*0.0625;

    // ── Log raw packet hex to MQTT ──
    {
      String raw;
      for (int i=0; i<bIndex; i++) {
        if (inData[i]<0x10) raw += "0";
        raw += String(inData[i], HEX) + " ";
      }
      raw.toUpperCase();
      mqttClient.publish(HASSIOHRVRAW, raw.c_str());
    }
    // ───────────────────────────────

    SendMQTTMessage();
    bStarted = bEnded = false;
    bIndex = 0;
  } else {
    myDelay(2000);
  }

  // Still-alive ping every 30 s
  if (WiFi.status()==WL_CONNECTED && mqttClient.connected() && iTotalDelay>=30000) {
    Serial.println("Telling HASSIO we’re still alive");
    mqttClient.publish(HASSIOHRVSTATUS, "1");
    iTotalDelay = 0;
  }

  mqttClient.loop();
}


// ----------------------------------------------------------------------------
// HELPERS
// ----------------------------------------------------------------------------

// Convert decimal to fixed-width hex string
String decToHex(byte val, byte width) {
  String s = String(val, HEX);
  while (s.length() < width) s = "0" + s;
  return s;
}

// Convert hex string to decimal
unsigned int hexToDec(String hex) {
  unsigned int v = 0;
  for (char c: hex) {
    int d = (c>='0'&&c<='9') ? c-'0'
          : (c>='A'&&c<='F') ? c-'A'+10
          : (c>='a'&&c<='f') ? c-'a'+10
          : 0;
    v = v*16 + d;
  }
  return v;
}

// Send parsed temps/control/fan over MQTT
void SendMQTTMessage() {
  if (WiFi.status()!=WL_CONNECTED || !mqttClient.connected()) return;

  // House/Roof temp to nearest 0.5°
  int tmp = int(fHRVTemp*2 + 0.5);
  fHRVTemp = tmp/2.0;
  pubString = String(fHRVTemp);
  pubString.toCharArray(message_buff, pubString.length()+1);

  if (eTempLoc==HRVHOUSE) {
    Serial.print("House "); Serial.println(message_buff);
    if (fHRVTemp != fHRVLastHouse) {
      fHRVLastHouse = fHRVTemp;
      mqttClient.publish(HASSIOHRVSUBHOUSE, message_buff);
    }
  }
  else if (eTempLoc==HRVROOF) {
    Serial.print("Roof "); Serial.println(message_buff);
    if (fHRVTemp != fHRVLastRoof) {
      fHRVLastRoof = fHRVTemp;
      mqttClient.publish(HASSIOHRVSUBROOF, message_buff);
    }
  }

  // Control set-temp (whole °) & fan speed
  if (iHRVControlTemp != iHRVLastControl || iHRVLastFanSpeed==255) {
    pubString = String(iHRVControlTemp);
    pubString.toCharArray(message_buff, pubString.length()+1);
    iHRVLastControl = iHRVControlTemp;
    Serial.print("Control Panel: "); Serial.println(message_buff);
    mqttClient.publish(HASSIOHRVSUBCONTROL, message_buff);
  }

  // Fan speed string
  if (iHRVFanSpeed != iHRVLastFanSpeed || iHRVControlTemp!=iHRVLastControl
      || (eTempLoc==HRVHOUSE && fHRVTemp!=fHRVLastHouse)
      || (eTempLoc==HRVROOF  && fHRVTemp!=fHRVLastRoof))
  {
    iHRVLastFanSpeed = iHRVFanSpeed;
    if (iHRVFanSpeed==0)         pubString="Off";
    else if (iHRVFanSpeed==5)    pubString="Idle";
    else if (iHRVFanSpeed==100) {
      pubString="Full";
      // heating/cooling note
      if (iHRVLastControl>=fHRVLastRoof && fHRVLastRoof>fHRVLastHouse)
        pubString += " (heating)";
      else if (fHRVLastRoof<=iHRVLastControl && fHRVLastRoof<fHRVLastHouse)
        pubString += " (cooling)";
    }
    else pubString = String(iHRVFanSpeed) + "%";

    pubString.toCharArray(message_buff, pubString.length()+1);
    Serial.print("Fan speed: "); Serial.println(message_buff);
    mqttClient.publish(HASSIOHRVSUBFANSPEED, message_buff);
  }

  // flash LED to show MQTT work
  digitalWrite(LED_BUILTIN, LOW);  myDelay(50);
  digitalWrite(LED_BUILTIN, HIGH); myDelay(1000);
}

// Yield-friendly delay
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

// Ensure Wi-Fi is connected (blocks until up or resets)
void startWIFI() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.println("Starting WIFI connection");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.begin(ssid, password);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    if (++tries > 450) ESP.reset();
  }
  Serial.print("WiFi connected, IP=");
  Serial.println(WiFi.localIP());
  myDelay(1500);
}
