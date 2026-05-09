/*
  HRV Keypad Passive Sniffer + JSON Profile Builder
  -------------------------------------------------
  Connect ESP32 RX-only to the HRV/keypad bus. Original keypad stays connected.
  ESP32 only listens and never transmits HRV commands.

  Wiring:
    HRV - / GND  -> ESP32 GND
    HRV data/bus -> ESP32 RX pin through your normal safe sniff input circuit
    ESP32 TX     -> NOT CONNECTED to HRV bus for passive sniffing

  Default serial:
    1200 baud, 8N1, 0x7E framed packets, negative-sum checksum.

  JSON stores PAYLOAD ONLY:
    Full sniffed frame: 7E 31 01 90 00 18 84 70 32 7E
    Stored payload:     31 01 90 00 18 84 70
1. Click Start boot capture.
2. Power-cycle the HRV/keypad.
3. Wait 45 seconds.
4. Click Learn Fan Low, then press low on the real keypad.
5. Click Learn Fan Medium, then press medium.
6. Click Learn Fan High, then press high.
7. Optional: learn fan off/on.
8. Download JSON profile.

*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <vector>

#define HRV_RX_PIN 16       // Change to suit sniffer ESP32 wiring
#define HRV_TX_PIN 17       // NOT CONNECTED. Required by HardwareSerial begin only.
#define HRV_BAUD   1200

static const char* AP_SSID = "HRV-Sniffer";
static const char* AP_PASS = "12345678";

// Optional STA Wi-Fi. If saved SSID is set, the sniffer runs AP + STA together.
// First boot still works as AP at http://192.168.4.1
Preferences prefs;
static const char* NVS_NS = "sniffer";
static const char* KEY_WIFI_SSID = "wifi_ssid";
static const char* KEY_WIFI_PASS = "wifi_pass";
static String sta_ssid = "";
static String sta_pass = "";
static uint32_t wifi_last_try_ms = 0;
static const uint32_t WIFI_RETRY_MS = 30000;

HardwareSerial HRV(2);
WebServer web(80);

static const uint8_t  MAX_FRAME_LEN       = 32;
static const uint16_t MAX_RECENT_FRAMES   = 80;
static const uint16_t MAX_BOOT_PAYLOADS   = 80;
static const uint16_t MAX_LEARN_PAYLOADS  = 30;
static const uint32_t LEARN_WINDOW_MS     = 12000;
static const uint32_t BOOT_WINDOW_MS      = 45000;

struct FrameRecord {
  uint32_t ms;
  bool valid;
  String fullFrame;
  String payload;
  String checksum;
  uint16_t count;
};

struct PayloadRecord {
  String payload;
  uint16_t count;
};

static std::vector<FrameRecord> recentFrames;
static std::vector<PayloadRecord> bootPayloads;
static std::vector<PayloadRecord> learnedPayloads;

static String currentLearnAction = "";
static String lastCompletedAction = "";
static String fanLowPayload = "";
static String fanMedPayload = "";
static String fanHighPayload = "";
static String fanOffPayload = "";
static String fanOnPayload = "";

static bool bootCaptureActive = false;
static uint32_t bootCaptureEndMs = 0;
static bool learnActive = false;
static uint32_t learnEndMs = 0;
static uint32_t validFrameCount = 0;
static uint32_t invalidFrameCount = 0;
static uint32_t lastFrameMs = 0;

// Live decoded fan observation from sniffed bus traffic.
// -1 means unknown/not decoded yet.
static int sniffedFanPercent = -1;
static uint32_t sniffedFanMs = 0;
static String sniffedFanPayload = "";
static String sniffedFanSource = "unknown";

static String hex2(uint8_t b) {
  char buf[4];
  snprintf(buf, sizeof(buf), "%02X", b);
  return String(buf);
}

static String bytesToHex(const std::vector<uint8_t>& data, size_t start, size_t endExclusive) {
  String s;
  for (size_t i = start; i < endExclusive && i < data.size(); i++) {
    if (s.length()) s += " ";
    s += hex2(data[i]);
  }
  return s;
}

static int payloadByteCount(const String& payload) {
  if (!payload.length()) return 0;
  int spaces = 0;
  for (size_t i = 0; i < payload.length(); i++) if (payload[i] == ' ') spaces++;
  return spaces + 1;
}

static bool payloadLooksUseful(const String& payload) {
  return payloadByteCount(payload) >= 4;
}

static bool parseHexPayload(const String& payload, std::vector<uint8_t>& out) {
  out.clear();
  int n = payload.length();
  int i = 0;
  while (i < n) {
    while (i < n && isspace((unsigned char)payload[i])) i++;
    if (i >= n) break;

    int hi = -1, lo = -1;
    char c1 = payload[i++];
    if (c1 >= '0' && c1 <= '9') hi = c1 - '0';
    else if (c1 >= 'A' && c1 <= 'F') hi = c1 - 'A' + 10;
    else if (c1 >= 'a' && c1 <= 'f') hi = c1 - 'a' + 10;
    else return false;

    if (i >= n) return false;
    char c2 = payload[i++];
    if (c2 >= '0' && c2 <= '9') lo = c2 - '0';
    else if (c2 >= 'A' && c2 <= 'F') lo = c2 - 'A' + 10;
    else if (c2 >= 'a' && c2 <= 'f') lo = c2 - 'a' + 10;
    else return false;

    out.push_back((uint8_t)((hi << 4) | lo));

    while (i < n && isspace((unsigned char)payload[i])) i++;
  }
  return out.size() > 0;
}

static String bytesToHexRange(const std::vector<uint8_t>& data, size_t start, size_t endExclusive) {
  String s;
  for (size_t i = start; i < endExclusive && i < data.size(); i++) {
    if (s.length()) s += " ";
    s += hex2(data[i]);
  }
  return s;
}

static bool sameRange(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, size_t startA, size_t startB, size_t len) {
  if (a.size() < startA + len || b.size() < startB + len) return false;
  for (size_t i = 0; i < len; i++) {
    if (a[startA + i] != b[startB + i]) return false;
  }
  return true;
}

static String detectFanMode() {
  std::vector<uint8_t> lo, me, hi;
  if (!parseHexPayload(fanLowPayload, lo) || !parseHexPayload(fanMedPayload, me) || !parseHexPayload(fanHighPayload, hi)) {
    return "incomplete";
  }

  if (lo.size() == me.size() && me.size() == hi.size() && lo.size() >= 7 &&
      lo[0] == 0x31 && me[0] == 0x31 && hi[0] == 0x31 &&
      lo[1] == 0x01 && me[1] == 0x01 && hi[1] == 0x01 &&
      sameRange(lo, me, 4, 4, lo.size() - 4) &&
      sameRange(lo, hi, 4, 4, lo.size() - 4)) {

    // Original RV21-style percentage command:
    //   31 01 [4E/4F/50] [percent] 1E 84 F0
    // His suspected RV16-style fixed frame:
    //   31 01 [8F/90/91] 00 18 84 70
    //
    // If the percent byte changes between low/medium/high, treat as percent template.
    // If it stays fixed, treat as fixed-frame profile.
    if (lo[3] != me[3] || me[3] != hi[3]) {
      return "percent_template";
    }
    return "fixed_frame";
  }

  return "fixed_frame";
}

static String buildFanTemplateJson() {
  String mode = detectFanMode();
  if (mode != "percent_template") return "null";

  std::vector<uint8_t> lo, me, hi;
  if (!parseHexPayload(fanLowPayload, lo) || !parseHexPayload(fanMedPayload, me) || !parseHexPayload(fanHighPayload, hi)) {
    return "null";
  }

  String out = "{";
  out += "\"prefix\":\"" + jsonEscape(bytesToHexRange(lo, 0, 2)) + "\",";
  out += "\"low_code\":\"" + hex2(lo[2]) + "\",";
  out += "\"medium_code\":\"" + hex2(me[2]) + "\",";
  out += "\"high_code\":\"" + hex2(hi[2]) + "\",";
  out += "\"percent_index\":3,";
  out += "\"suffix\":\"" + jsonEscape(bytesToHexRange(lo, 4, lo.size())) + "\",";
  out += "\"example_low_percent\":" + String((int)lo[3]) + ",";
  out += "\"example_medium_percent\":" + String((int)me[3]) + ",";
  out += "\"example_high_percent\":" + String((int)hi[3]);
  out += "}";
  return out;
}

static bool payloadEqualsLearned(const String& a, const String& b) {
  return a.length() && b.length() && a.equalsIgnoreCase(b);
}

static bool tailMatches(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b, size_t start) {
  if (a.size() != b.size() || a.size() <= start || b.size() <= start) return false;
  for (size_t i = start; i < a.size(); i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

static bool decodeFanPercentFromPayload(const String& payload, int& pct, String& source) {
  pct = -1;
  source = "unknown";

  std::vector<uint8_t> rx;
  if (!parseHexPayload(payload, rx)) return false;

  String mode = detectFanMode();

  // Best case: learned percentage-template mode.
  // Example original HRV:
  //   31 01 [4E/4F/50] [percent] 1E 84 F0
  if (mode == "percent_template") {
    std::vector<uint8_t> lo, me, hi;
    if (parseHexPayload(fanLowPayload, lo) &&
        parseHexPayload(fanMedPayload, me) &&
        parseHexPayload(fanHighPayload, hi) &&
        rx.size() == lo.size() && rx.size() >= 7 &&
        rx[0] == lo[0] && rx[1] == lo[1] &&
        tailMatches(rx, lo, 4) &&
        (rx[2] == lo[2] || rx[2] == me[2] || rx[2] == hi[2]) &&
        rx[3] <= 100) {
      pct = (int)rx[3];
      source = "percent_template";
      return true;
    }
  }

  // Fixed-frame mode: exact learned frame matching.
  // This will not know a true arbitrary percentage, but it can show the selected band.
  if (payloadEqualsLearned(payload, fanOffPayload))  { pct = 0;   source = "fixed_off";  return true; }
  if (payloadEqualsLearned(payload, fanLowPayload))  { pct = 30;  source = "fixed_low";  return true; }
  if (payloadEqualsLearned(payload, fanMedPayload))  { pct = 50;  source = "fixed_med";  return true; }
  if (payloadEqualsLearned(payload, fanHighPayload)) { pct = 100; source = "fixed_high"; return true; }
  if (payloadEqualsLearned(payload, fanOnPayload))   { pct = 30;  source = "fixed_on";   return true; }

  // Useful fallback before learning is complete: detect the original RV21-style command directly.
  // This is intentionally conservative.
  if (rx.size() == 7 &&
      rx[0] == 0x31 && rx[1] == 0x01 &&
      (rx[2] == 0x4E || rx[2] == 0x4F || rx[2] == 0x50) &&
      rx[4] == 0x1E && rx[5] == 0x84 && rx[6] == 0xF0 &&
      rx[3] <= 100) {
    pct = (int)rx[3];
    source = "rv21_fallback";
    return true;
  }

  return false;
}

static void updateObservedFanFromPayload(const String& payload) {
  int pct = -1;
  String source;
  if (!decodeFanPercentFromPayload(payload, pct, source)) return;

  sniffedFanPercent = pct;
  sniffedFanMs = millis();
  sniffedFanPayload = payload;
  sniffedFanSource = source;

  Serial.print("FAN OBSERVED: ");
  Serial.print(sniffedFanPercent);
  Serial.print("% source=");
  Serial.print(sniffedFanSource);
  Serial.print(" payload=");
  Serial.println(sniffedFanPayload);
}

static bool isSamePayload(const String& a, const String& b) {
  return a.equalsIgnoreCase(b);
}

static void addOrIncPayload(std::vector<PayloadRecord>& list, const String& payload, uint16_t maxItems) {
  if (!payload.length()) return;
  for (auto &r : list) {
    if (isSamePayload(r.payload, payload)) {
      if (r.count < 65535) r.count++;
      return;
    }
  }
  if (list.size() >= maxItems) list.erase(list.begin());
  PayloadRecord pr{payload, 1};
  list.push_back(pr);
}

static void addRecentFrame(const FrameRecord& rec) {
  if (!recentFrames.empty()) {
    FrameRecord &last = recentFrames.back();
    if (last.valid == rec.valid && isSamePayload(last.payload, rec.payload) && last.checksum == rec.checksum) {
      if (last.count < 65535) last.count++;
      last.ms = rec.ms;
      return;
    }
  }
  if (recentFrames.size() >= MAX_RECENT_FRAMES) recentFrames.erase(recentFrames.begin());
  recentFrames.push_back(rec);
}

static void handleValidFrame(const std::vector<uint8_t>& frame) {
  size_t checksumIndex = frame.size() - 2;
  FrameRecord rec;
  rec.ms = millis();
  rec.valid = true;
  rec.fullFrame = bytesToHex(frame, 0, frame.size());
  rec.payload = bytesToHex(frame, 1, checksumIndex);
  rec.checksum = hex2(frame[checksumIndex]);
  rec.count = 1;

  validFrameCount++;
  lastFrameMs = rec.ms;

  Serial.print("VALID "); Serial.print(rec.ms); Serial.print("ms FRAME: "); Serial.println(rec.fullFrame);
  addRecentFrame(rec);
  updateObservedFanFromPayload(rec.payload);

  if (bootCaptureActive) addOrIncPayload(bootPayloads, rec.payload, MAX_BOOT_PAYLOADS);
  if (learnActive && payloadLooksUseful(rec.payload)) addOrIncPayload(learnedPayloads, rec.payload, MAX_LEARN_PAYLOADS);
}

static void handleInvalidFrame(const std::vector<uint8_t>& frame) {
  FrameRecord rec;
  rec.ms = millis();
  rec.valid = false;
  rec.fullFrame = bytesToHex(frame, 0, frame.size());
  rec.payload = frame.size() >= 3 ? bytesToHex(frame, 1, frame.size() - 2) : "";
  rec.checksum = frame.size() >= 2 ? hex2(frame[frame.size() - 2]) : "";
  rec.count = 1;

  invalidFrameCount++;
  lastFrameMs = rec.ms;

  Serial.print("BAD   "); Serial.print(rec.ms); Serial.print("ms FRAME: "); Serial.println(rec.fullFrame);
  addRecentFrame(rec);
}

static void process_uart() {
  static std::vector<uint8_t> frame;
  while (HRV.available()) {
    uint8_t b = HRV.read();
    if (frame.empty()) {
      if (b == 0x7E) frame.push_back(b);
      continue;
    }
    frame.push_back(b);
    if (b == 0x7E && frame.size() >= 4) {
      if (frame.size() >= 5) {
        size_t checksumIndex = frame.size() - 2;
        int sum = 0;
        for (size_t i = 1; i < checksumIndex; i++) sum -= frame[i];
        bool ok = ((uint8_t)(sum & 0xFF) == frame[checksumIndex]);
        if (ok) handleValidFrame(frame);
        else handleInvalidFrame(frame);
      } else {
        handleInvalidFrame(frame);
      }
      frame.clear();
    }
    if (frame.size() > MAX_FRAME_LEN) frame.clear();
  }
}

static void serviceCaptureWindows() {
  uint32_t now = millis();
  if (bootCaptureActive && (int32_t)(now - bootCaptureEndMs) >= 0) {
    bootCaptureActive = false;
    Serial.println("BOOT CAPTURE FINISHED");
  }
  if (learnActive && (int32_t)(now - learnEndMs) >= 0) {
    learnActive = false;
    lastCompletedAction = currentLearnAction;

    String best = "";
    uint16_t bestCount = 0;
    for (auto &r : learnedPayloads) {
      if (payloadLooksUseful(r.payload) && r.count > bestCount) {
        best = r.payload;
        bestCount = r.count;
      }
    }

    if (best.length()) {
      if      (currentLearnAction == "fan_low")  fanLowPayload  = best;
      else if (currentLearnAction == "fan_med")  fanMedPayload  = best;
      else if (currentLearnAction == "fan_high") fanHighPayload = best;
      else if (currentLearnAction == "fan_off")  fanOffPayload  = best;
      else if (currentLearnAction == "fan_on")   fanOnPayload   = best;
    }

    Serial.print("LEARN FINISHED: "); Serial.print(currentLearnAction); Serial.print(" best="); Serial.println(best);
    currentLearnAction = "";
  }
}

static String jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else out += c;
  }
  return out;
}

static void appendPayloadArray(String& out, const std::vector<PayloadRecord>& list, bool includeCounts) {
  out += "[";
  for (size_t i = 0; i < list.size(); i++) {
    if (i) out += ",";
    if (includeCounts) {
      out += "{\"payload\":\"" + jsonEscape(list[i].payload) + "\",\"count\":" + String(list[i].count) + "}";
    } else {
      out += "\"" + jsonEscape(list[i].payload) + "\"";
    }
  }
  out += "]";
}

static String buildProfileJson() {
  String fanMode = detectFanMode();
  String out;
  out.reserve(10000);
  out += "{\n";
  out += "  \"profile_name\": \"HRV learned profile\",\n";
  out += "  \"format\": \"payload_only_no_7E_no_checksum\",\n";
  out += "  \"baud\": " + String(HRV_BAUD) + ",\n";
  out += "  \"serial\": \"8N1\",\n";
  out += "  \"checksum\": \"negative_sum_payload_bytes\",\n";
  out += "  \"notes\": \"Payloads only. Firmware should wrap with 7E and calculate checksum.\",\n";
  out += "  \"startup_sequence\": ";
  appendPayloadArray(out, bootPayloads, false);
  out += ",\n";
  out += "  \"fan_mode\": \"" + fanMode + "\",\n";
  out += "  \"fan_template\": ";
  out += buildFanTemplateJson();
  out += ",\n";
  out += "  \"fan\": {\n";
  out += "    \"low\": \"" + jsonEscape(fanLowPayload) + "\",\n";
  out += "    \"medium\": \"" + jsonEscape(fanMedPayload) + "\",\n";
  out += "    \"high\": \"" + jsonEscape(fanHighPayload) + "\",\n";
  out += "    \"off\": \"" + jsonEscape(fanOffPayload) + "\",\n";
  out += "    \"on\": \"" + jsonEscape(fanOnPayload) + "\"\n";
  out += "  },\n";
  out += "  \"fan_mode_notes\": {\n";
  out += "    \"percent_template\": \"Use prefix + speed code + requested percent byte + suffix for true 0-100 control.\",\n";
  out += "    \"fixed_frame\": \"Replay learned low/medium/high payloads. Slider should map to speed bands.\",\n";
  out += "    \"incomplete\": \"Learn low, medium, and high before importing into controller firmware.\"\n";
  out += "  },\n";
  out += "  \"learned_candidates\": {\n";
  out += "    \"last_action\": \"" + jsonEscape(lastCompletedAction) + "\",\n";
  out += "    \"last_candidates\": ";
  appendPayloadArray(out, learnedPayloads, true);
  out += "\n  },\n";
  out += "  \"stats\": {\n";
  out += "    \"valid_frames\": " + String(validFrameCount) + ",\n";
  out += "    \"invalid_frames\": " + String(invalidFrameCount) + ",\n";
  out += "    \"last_frame_ms\": " + String(lastFrameMs) + "\n";
  out += "  }\n";
  out += "}\n";
  return out;
}

static String buildRecentJson() {
  String out;
  out.reserve(9000);
  out += "{";
  out += "\"valid\":" + String(validFrameCount) + ",";
  out += "\"invalid\":" + String(invalidFrameCount) + ",";
  out += "\"boot_active\":" + String(bootCaptureActive ? "true" : "false") + ",";
  out += "\"learn_active\":" + String(learnActive ? "true" : "false") + ",";
  out += "\"learn_action\":\"" + jsonEscape(currentLearnAction) + "\",";
  out += "\"fan_mode\":\"" + detectFanMode() + "\",";
  out += "\"fan_percent\":" + String(sniffedFanPercent) + ",";
  out += "\"fan_payload\":\"" + jsonEscape(sniffedFanPayload) + "\",";
  out += "\"fan_source\":\"" + jsonEscape(sniffedFanSource) + "\",";
  out += "\"fan_age_ms\":" + String(sniffedFanMs ? (millis() - sniffedFanMs) : 0) + ",";
  out += "\"wifi_mode\":\"" + String(WiFi.isConnected() ? "AP+STA" : "AP") + "\",";
  out += "\"sta_ssid\":\"" + jsonEscape(sta_ssid) + "\",";
  out += String("\"sta_ip\":\"") + (WiFi.isConnected() ? WiFi.localIP().toString() : String("")) + "\",";
  out += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
  out += "\"rssi\":" + String(WiFi.isConnected() ? WiFi.RSSI() : 0) + ",";
  out += "\"frames\":[";
  for (size_t i = 0; i < recentFrames.size(); i++) {
    if (i) out += ",";
    out += "{";
    out += "\"ms\":" + String(recentFrames[i].ms) + ",";
    out += "\"valid\":" + String(recentFrames[i].valid ? "true" : "false") + ",";
    out += "\"count\":" + String(recentFrames[i].count) + ",";
    out += "\"payload\":\"" + jsonEscape(recentFrames[i].payload) + "\",";
    out += "\"checksum\":\"" + jsonEscape(recentFrames[i].checksum) + "\",";
    out += "\"full\":\"" + jsonEscape(recentFrames[i].fullFrame) + "\"";
    out += "}";
  }
  out += "]}";
  return out;
}

static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>HRV Keypad Sniffer</title>
<style>
body{font-family:system-ui,Arial;margin:0;background:#f5f7fa;color:#222}header{background:#263238;color:#fff;padding:12px 16px}main{max-width:980px;margin:auto;padding:16px}.card{background:#fff;border-radius:10px;padding:14px 16px;margin-bottom:14px;box-shadow:0 2px 6px rgba(0,0,0,.08)}button,a.btn{display:inline-block;border:0;border-radius:7px;background:#1976d2;color:#fff;padding:9px 12px;margin:4px;cursor:pointer;text-decoration:none;font:inherit}button.warn{background:#ef6c00}button.bad{background:#c62828}button.ok{background:#2e7d32}pre{background:#111;color:#0f0;padding:12px;border-radius:8px;overflow:auto;max-height:420px}table{width:100%;border-collapse:collapse;font-size:13px}td,th{border-bottom:1px solid #eee;padding:5px;text-align:left;vertical-align:top}small{color:#666}.goodtext{color:#2e7d32;font-weight:bold}input{width:100%;box-sizing:border-box;padding:8px;margin:3px 0 8px;border:1px solid #ccc;border-radius:6px}label{font-weight:600;font-size:14px}
</style></head><body><header><h2>HRV Keypad Passive Sniffer</h2></header><main>
<div class="card"><b>Status:</b> Valid frames: <span id="valid">0</span> | Invalid: <span id="invalid">0</span> | Boot capture: <span id="boot">off</span> | Learn: <span id="learn">off</span> | Fan mode: <span id="fanmode">incomplete</span><br>Observed fan: <span id="fanpct">--</span>% | Source: <span id="fansrc">unknown</span> | Payload: <code><span id="fanpayload"></span></code><br>WiFi: <span id="wmode">AP</span> | AP: <span id="apip">192.168.4.1</span> | STA: <span id="staip"></span> | RSSI: <span id="rssi">0</span><p><small>Connect ESP RX-only to the bus. Keep ESP TX disconnected from HRV.</small></p></div>
<div class="card"><h3>Network Wi-Fi</h3><p><small>First connect to AP <b>HRV-Sniffer</b> / <b>12345678</b>, save your home Wi-Fi, then use the shown STA IP on your network. AP stays enabled as fallback.</small></p><label>Wi-Fi SSID</label><input id="wfssid" placeholder="Your Wi-Fi SSID"><label>Password</label><input id="wfpw" type="password" placeholder="Wi-Fi password"><button class="ok" onclick="saveWifi()">Save Wi-Fi & Connect</button><button class="bad" onclick="clearWifi()">Forget Wi-Fi</button> <span id="wfmsg"></span></div>
<div class="card"><h3>1. Startup Capture</h3><p>Click this, then power-cycle the HRV/keypad. It captures unique startup payloads for 45 seconds.</p><button class="ok" onclick="cmd('/start_boot')">Start boot capture</button><button class="bad" onclick="cmd('/clear_boot')">Clear startup</button></div>
<div class="card"><h3>2. Fan Button Learning</h3><p>Click an action, then press that button on the real keypad within 12 seconds.</p><button onclick="learn('fan_low')">Learn Fan Low</button><button onclick="learn('fan_med')">Learn Fan Medium</button><button onclick="learn('fan_high')">Learn Fan High</button><button class="warn" onclick="learn('fan_off')">Learn Fan Off</button><button class="warn" onclick="learn('fan_on')">Learn Fan On</button><button class="bad" onclick="cmd('/clear_learn')">Clear last candidates</button></div>
<div class="card"><h3>3. Export JSON</h3><a class="btn" href="/profile" target="_blank">View JSON profile</a><a class="btn" href="/download">Download JSON profile</a><button onclick="copyProfile()">Copy JSON to clipboard</button><pre id="profile">{}</pre></div>
<div class="card"><h3>Recent Frames</h3><button onclick="cmd('/clear_recent')">Clear recent</button><table><thead><tr><th>ms</th><th>OK</th><th>Count</th><th>Payload only</th><th>Checksum</th><th>Full frame</th></tr></thead><tbody id="frames"></tbody></table></div>
</main><script>
async function getJson(url){return fetch(url).then(r=>r.json())} async function getText(url){return fetch(url).then(r=>r.text())} async function cmd(url){await fetch(url); await tick()} async function learn(action){await fetch('/learn?action='+encodeURIComponent(action)); await tick()}
async function copyProfile(){
  const txt = await getText('/profile');

  // navigator.clipboard often fails on ESP web pages served over plain HTTP/IP.
  // Try it first, then fall back to a hidden textarea copy method.
  try {
    if (navigator.clipboard && window.isSecureContext) {
      await navigator.clipboard.writeText(txt);
      alert('JSON profile copied');
      return;
    }
  } catch(e) {}

  try {
    const ta = document.createElement('textarea');
    ta.value = txt;
    ta.setAttribute('readonly', '');
    ta.style.position = 'fixed';
    ta.style.left = '-9999px';
    ta.style.top = '0';
    document.body.appendChild(ta);
    ta.focus();
    ta.select();
    ta.setSelectionRange(0, ta.value.length);
    const ok = document.execCommand('copy');
    document.body.removeChild(ta);

    if (ok) alert('JSON profile copied');
    else alert('Copy failed. Use Download JSON profile instead.');
  } catch(e) {
    alert('Copy failed. Use Download JSON profile instead.');
  }
}
async function loadWifi(){try{const w=await getJson('/wifi'); document.getElementById('wfssid').value=w.ssid||''; document.getElementById('wfpw').value=w.pass||'';}catch(e){}}
async function saveWifi(){const ssid=encodeURIComponent(document.getElementById('wfssid').value.trim()); const pass=encodeURIComponent(document.getElementById('wfpw').value); document.getElementById('wfmsg').textContent='Saving...'; try{const r=await getJson('/wifi?ssid='+ssid+'&pass='+pass); document.getElementById('wfmsg').textContent=r.ok?'Saved. Connecting...':'Failed'; setTimeout(tick,1500);}catch(e){document.getElementById('wfmsg').textContent='Error';}}
async function clearWifi(){if(!confirm('Forget saved Wi-Fi and use AP only?'))return; await getJson('/wifi_clear'); document.getElementById('wfssid').value=''; document.getElementById('wfpw').value=''; document.getElementById('wfmsg').textContent='Wi-Fi forgotten'; setTimeout(tick,1000);}
async function tick(){try{const s=await getJson('/recent'); document.getElementById('valid').textContent=s.valid; document.getElementById('invalid').textContent=s.invalid; document.getElementById('boot').innerHTML=s.boot_active?'<span class=goodtext>capturing</span>':'off'; document.getElementById('learn').innerHTML=s.learn_active?'<span class=goodtext>'+s.learn_action+'</span>':'off'; document.getElementById('fanmode').textContent=s.fan_mode||'incomplete'; document.getElementById('fanpct').textContent=(s.fan_percent>=0?s.fan_percent:'--'); document.getElementById('fansrc').textContent=s.fan_source||'unknown'; document.getElementById('fanpayload').textContent=s.fan_payload||''; document.getElementById('wmode').textContent=s.wifi_mode||''; document.getElementById('apip').textContent=s.ap_ip||''; document.getElementById('staip').textContent=s.sta_ip||'(not connected)'; document.getElementById('rssi').textContent=s.rssi||0; const tb=document.getElementById('frames'); tb.innerHTML=''; for(const f of s.frames.slice().reverse()){const tr=document.createElement('tr'); tr.innerHTML='<td>'+f.ms+'</td><td>'+(f.valid?'✅':'❌')+'</td><td>'+f.count+'</td><td><code>'+f.payload+'</code></td><td><code>'+f.checksum+'</code></td><td><code>'+f.full+'</code></td>'; tb.appendChild(tr)}}catch(e){} try{document.getElementById('profile').textContent=await getText('/profile')}catch(e){}}
loadWifi(); setInterval(tick,1000); tick();
</script></body></html>
)HTML";

static void handleRoot() { web.send_P(200, "text/html", INDEX_HTML); }
static void handleRecent() { web.send(200, "application/json", buildRecentJson()); }
static void handleProfile() { web.send(200, "application/json", buildProfileJson()); }
static void handleDownload() { web.sendHeader("Content-Disposition", "attachment; filename=hrv_profile.json"); web.send(200, "application/json", buildProfileJson()); }
static void handleStartBoot() { bootPayloads.clear(); bootCaptureActive = true; bootCaptureEndMs = millis() + BOOT_WINDOW_MS; Serial.println("BOOT CAPTURE STARTED"); web.send(200, "application/json", "{\"ok\":true,\"boot_capture\":true}"); }
static void handleClearBoot() { bootPayloads.clear(); bootCaptureActive = false; web.send(200, "application/json", "{\"ok\":true}"); }
static void handleLearn() {
  String action = web.arg("action");
  if (!(action == "fan_low" || action == "fan_med" || action == "fan_high" || action == "fan_off" || action == "fan_on")) { web.send(400, "application/json", "{\"ok\":false,\"error\":\"bad_action\"}"); return; }
  learnedPayloads.clear(); currentLearnAction = action; learnActive = true; learnEndMs = millis() + LEARN_WINDOW_MS;
  Serial.print("LEARN STARTED: "); Serial.println(action);
  web.send(200, "application/json", "{\"ok\":true,\"learn\":\"" + action + "\"}");
}
static void handleClearLearn() { learnedPayloads.clear(); learnActive = false; currentLearnAction = ""; web.send(200, "application/json", "{\"ok\":true}"); }
static void handleClearRecent() { recentFrames.clear(); web.send(200, "application/json", "{\"ok\":true}"); }

static String wifiJson() {
  String out = "{";
  out += "\"ok\":true,";
  out += "\"ssid\":\"" + jsonEscape(sta_ssid) + "\",";
  out += "\"pass\":\"" + jsonEscape(sta_pass) + "\",";
  out += "\"connected\":" + String(WiFi.isConnected() ? "true" : "false") + ",";
  out += String("\"sta_ip\":\"") + (WiFi.isConnected() ? WiFi.localIP().toString() : String("")) + "\",";
  out += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\"";
  out += "}";
  return out;
}

static void wifiConnectNow() {
  if (sta_ssid.length() == 0) return;
  WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
  wifi_last_try_ms = millis();
  Serial.print("Connecting STA Wi-Fi to: ");
  Serial.println(sta_ssid);
}

static void handleWifi() {
  if (web.hasArg("ssid")) {
    sta_ssid = web.arg("ssid");
    sta_pass = web.arg("pass");
    sta_ssid.trim();
    prefs.putString(KEY_WIFI_SSID, sta_ssid);
    prefs.putString(KEY_WIFI_PASS, sta_pass);
    if (sta_ssid.length()) wifiConnectNow();
  }
  web.send(200, "application/json", wifiJson());
}

static void handleWifiClear() {
  sta_ssid = "";
  sta_pass = "";
  prefs.remove(KEY_WIFI_SSID);
  prefs.remove(KEY_WIFI_PASS);
  WiFi.disconnect(false, true);
  web.send(200, "application/json", "{\"ok\":true}");
}

static void wifiService() {
  if (sta_ssid.length() == 0) return;
  if (WiFi.isConnected()) return;
  if (millis() - wifi_last_try_ms > WIFI_RETRY_MS) {
    wifiConnectNow();
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("HRV Keypad Passive Sniffer");
  Serial.println("RX-only mode. Do not connect ESP TX to HRV bus.");
  Serial.print("RX pin: "); Serial.println(HRV_RX_PIN);
  Serial.print("Baud: "); Serial.println(HRV_BAUD);
  HRV.begin(HRV_BAUD, SERIAL_8N1, HRV_RX_PIN, HRV_TX_PIN);
  prefs.begin(NVS_NS, false);
  sta_ssid = prefs.getString(KEY_WIFI_SSID, "");
  sta_pass = prefs.getString(KEY_WIFI_PASS, "");

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("AP SSID: "); Serial.println(AP_SSID);
  Serial.print("AP PASS: "); Serial.println(AP_PASS);
  Serial.print("AP Open: http://"); Serial.println(WiFi.softAPIP());
  if (sta_ssid.length()) {
    wifiConnectNow();
    Serial.println("STA Wi-Fi saved. Watch serial or web UI for network IP.");
  } else {
    Serial.println("No STA Wi-Fi saved. Use AP web UI to configure network Wi-Fi.");
  }
  web.on("/", HTTP_GET, handleRoot);
  web.on("/recent", HTTP_GET, handleRecent);
  web.on("/profile", HTTP_GET, handleProfile);
  web.on("/download", HTTP_GET, handleDownload);
  web.on("/start_boot", HTTP_GET, handleStartBoot);
  web.on("/clear_boot", HTTP_GET, handleClearBoot);
  web.on("/learn", HTTP_GET, handleLearn);
  web.on("/clear_learn", HTTP_GET, handleClearLearn);
  web.on("/clear_recent", HTTP_GET, handleClearRecent);
  web.on("/wifi", HTTP_GET, handleWifi);
  web.on("/wifi_clear", HTTP_GET, handleWifiClear);
  web.begin();
  Serial.println("Web server started.");
}

void loop() {
  web.handleClient();
  wifiService();
  process_uart();
  serviceCaptureWindows();
}
