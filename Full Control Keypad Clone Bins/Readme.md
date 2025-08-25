# HRV Touchscreen Controller

<img width="1099" alt="UI Screens" src="https://github.com/user-attachments/assets/15ab0996-d049-4504-8225-8518f2b27659" />
<img width="927" height="276" alt="image" src="https://github.com/user-attachments/assets/de8ad6a1-b9fd-489c-b64a-ffe017552fab" />


ESP32-based **touchscreen controller** for HRV (Heat Recovery Ventilation) systems.  
Built with **LVGL**, **TFT_eSPI**, and **XPT2046 touch**, with integrated Wi-Fi, MQTT, Web UI, SD card logging, and Home Assistant auto-discovery.

---

## ✨ Features

### Full LVGL Touchscreen UI (240×320, 16-bit)
- Live tiles for:
  • House & roof temperature  
  • Humidity  
  • Fan % (target & actual)  
  • Filter life (% & days)  
  • Wi-Fi / MQTT status  
  • Clock  
- Context icons (sun/cold, roof-air indicators).  
- Soft buzzer feedback.  
- Background image & theming support.  

### Auto Ventilation with Setpoint
- AUTO mode uses setpoint °C vs house/roof temps.  
- Deadband, proportional gain, and trickle/minimums keep control smooth.  
- If temps unknown → AUTO falls back to 30% fan.  

### Manual Controls & Overrides
- Fan slider → immediate override for 20 min.  
- Power toggle → full ON/OFF (UI + MQTT).  
- “Burnt Toast” boost → 100% for a timed burst, then safe fallback.  

### Sensors
- SHT31 (I²C) for indoor temp & humidity.  
- HRV UART for roof temp + fan actual %.  
- Sensor timeouts handled gracefully.  

### HRV Bus Protocol
- Half-duplex framed comms with checksum.  
- Boot handshake + periodic keepalives.  
- Roof probe until valid frame received.  

### Wi-Fi + Web UI
- Tries STA first → falls back to captive AP (HRV-Keypad/12345678) if STA fails.  
- Built-in WebServer:  
  • /status JSON snapshot  
  • Control endpoints (/api/power, /api/boost, /api/setfan, /api/setpoint, /api/auto)  
  • Config pages (/mqtt, /wifi)  
- mDNS (http://hrv-keypad.local) and NTP with NZ timezone.  

### MQTT (Home Assistant Native)
- HA Discovery with retained states:  
  • Sensors: temp, humidity, fan %, filter days/life, boost remaining  
  • Switches: Power, Boost  
  • Numbers: Fan %, Setpoint °C, Filter days  
- Topics: hassio/touchscreen_hrv/...  
- LWT: publishes online/offline.  

### Persistence
- NVS (Preferences): Wi-Fi, MQTT, setpoint, filter counters.  
- SD card (optional):  
  • /hrv/setpoint.json (30s debounce).  
  • /hrv/filter.json (days + epoch anchor).  
  • /hrv/status.json (hourly snapshot).  
- Daily decrement of filter life at midnight.  
- Reset button: restores 730 days, re-anchors, syncs via MQTT + SD.  

### UX Details
- Hysteresis to prevent jitter.  
- UI refresh timers for info & clock.  
- Hourly status logs to SD.  

---

## 🌐 Web API
- `GET /`                   → Status/control page (HTML)  
- `GET /status`             → JSON snapshot  
- `GET /api/power?toggle=1` → Toggle power  
- `GET /api/boost?toggle=1` → Toggle Burnt Toast  
- `GET /api/setfan?val=NN`  → Set fan % (0–100)  
- `GET /api/setpoint?c=NN`  → Set setpoint °C (5–35)  
- `GET /api/auto?mode=...`  → Switch AUTO / manual  
- `GET /mqtt`               → Get/set MQTT config  
- `GET /wifi`               → Get/set Wi-Fi config  

---

## 📡 MQTT Topics

### State
- .../house_temp/state  
- .../house_humidity/state  
- .../roof_temp/state  
- .../fan_percent/state  
- .../setpoint/state  
- .../filter_days_remaining/state  
- .../filter_life/state  
- .../boost_active/state  
- .../boost_remaining_s/state  
- .../power/state  
- .../status   (online/offline LWT)  

### Commands
- .../power/set ("ON"/"OFF")  
- .../boost/set ("ON"/"OFF")  
- .../fan_percent/state (number)  
- .../setpoint/set (°C)  
- .../filter_days_remaining/set (0–2000)  

---

## ⚙️ Hardware / Platform
- ESP32 (ESP32-D0WD-V3), 4 MB flash  
- TFT_eSPI + XPT2046 touchscreen (240×320)  
- SHT31 on SDA=27, SCL=22  
- SD card on CS=5 (VSPI)  
- HRV UART on RX=35, TX=1 @ 1200 bps  

---

## 🔌 Flashing

Arduino IDE (or ESP-IDF) build outputs:  
- `bootloader.bin`  
- `partitions.bin`  
- `app.bin`  

### Option A (Recommended: Merged)
- Flash `merged.bin` → @0x0  

### Option B (Three-bin layout)
- `bootloader.bin`  → 0x1000  
- `partitions.bin`  → 0x8000  
- `app.bin`         → 0x10000  

SPI Mode: DIO  
Speed:    40 MHz  
Erase:    Yes  

⚠️ Don’t forget to **power-cycle** after flashing.

<img width="867" alt="Flash Tool" src="https://github.com/user-attachments/assets/a51058e4-aef6-4372-af57-780177bdcd26" />
