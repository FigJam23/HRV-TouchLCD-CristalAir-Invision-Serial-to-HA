# HRV Touchscreen Controller
<img src="https://github.com/user-attachments/assets/5137771c-f3e0-4be5-ac15-6035f58d54ce" width="240" height="320">

<img src="https://github.com/user-attachments/assets/23fbf5c9-0abc-48af-8022-2cfee745771f" width="240" height="320">


<img width="1099" alt="UI Screens" src="https://github.com/user-attachments/assets/15ab0996-d049-4504-8225-8518f2b27659" />



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




<img src="https://github.com/user-attachments/assets/a51058e4-aef6-4372-af57-780177bdcd26" width="800" height="650">


```
type: vertical-stack
cards:
  # ===== HEADER =====
  - type: custom:mushroom-title-card
    title: HRV – Mobile

  # ===== QUICK STATUS (includes Boost Remaining) =====
  - type: custom:mushroom-chips-card
    alignment: justify
    chips:
      # Power (tap to toggle)
      - type: template
        entity: switch.hrv_hrv_power
        icon: >
          {% if is_state('switch.hrv_hrv_power','on') %}mdi:power{% else %}mdi:power-off{% endif %}
        icon_color: >
          {% if is_state('switch.hrv_hrv_power','on') %}green{% else %}grey{% endif %}
        content: >
          {% if is_state('switch.hrv_hrv_power','on') %}On{% else %}Off{% endif %}
        tap_action: { action: toggle }

      # Boost state + remaining
      - type: template
        entity: binary_sensor.hrv_boost_active_2
        icon: mdi:rocket-launch
        icon_color: >
          {% if is_state('binary_sensor.hrv_boost_active_2','on') %}purple{% else %}grey{% endif %}
        content: >
          {% if is_state('binary_sensor.hrv_boost_active_2','on') %}
            Boost {{ states('sensor.hrv_hrv_boost_remaining') }}
          {% else %}Boost Off{% endif %}

      # Filter needed?
      - type: template
        entity: binary_sensor.hrv_filter_replacement_needed_2
        icon: mdi:air-filter
        icon_color: >
          {% if is_state('binary_sensor.hrv_filter_replacement_needed_2','on') %}red{% else %}green{% endif %}
        content: >
          {% if is_state('binary_sensor.hrv_filter_replacement_needed_2','on') %}Filter Needed{% else %}Filter OK{% endif %}

      # House temp
      - type: template
        entity: sensor.hrv_house_temp_2
        icon: mdi:home-thermometer
        icon_color: >
          {% set t = states('sensor.hrv_house_temp_2')|float(0) %}
          {% if t < 15 %}blue{% elif t < 22 %}green{% elif t < 27 %}amber{% else %}red{% endif %}
        content: "{{ states('sensor.hrv_house_temp_2') }}°C"

      # Roof temp
      - type: template
        entity: sensor.hrv_hrv_roof_temp
        icon: mdi:home-roof
        icon_color: >
          {% set t = states('sensor.hrv_hrv_roof_temp')|float(0) %}
          {% if t < 10 %}blue{% elif t < 25 %}green{% elif t < 35 %}amber{% else %}red{% endif %}
        content: "{{ states('sensor.hrv_hrv_roof_temp') }}°C"

      # Humidity
      - type: template
        entity: sensor.hrv_house_humidity_2
        icon: mdi:water-percent
        icon_color: >
          {% set h = states('sensor.hrv_house_humidity_2')|float(0) %}
          {% if h < 35 %}blue{% elif h < 60 %}green{% else %}amber{% endif %}
        content: "{{ states('sensor.hrv_house_humidity_2') }}%"

  # ===== CONTROLS =====
  - type: custom:stack-in-card
    mode: vertical
    cards:
      - type: horizontal-stack
        cards:
          - type: custom:mushroom-number-card
            entity: number.hrv_hrv_setpoint
            name: Set °C
            icon: mdi:thermometer-auto
            display_mode: slider
            fill_container: true
          - type: custom:mushroom-number-card
            entity: number.hrv_hrv_fan
            name: Fan %
            icon: mdi:fan
            display_mode: slider
            fill_container: true
      - type: horizontal-stack
        cards:
          - type: custom:mushroom-entity-card
            entity: switch.hrv_hrv_boost
            name: Boost
            icon: mdi:rocket
            tap_action: { action: toggle }
          - type: custom:mushroom-entity-card
            entity: switch.hrv_hrv_power
            name: Power
            icon: mdi:power
            tap_action: { action: toggle }
      - type: custom:mushroom-entity-card
        entity: number.hrv_hrv_filter_days_remaining
        name: Set Filter Days
        icon: mdi:calendar-clock

  # ===== GRAPHS SIDE BY SIDE =====
  - type: horizontal-stack
    cards:
      - type: custom:mini-graph-card
        name: House Temp (24h)
        entities: [ sensor.hrv_house_temp_2 ]
        hours_to_show: 24
        points_per_hour: 4
        line_width: 3
        smoothing: true
        show:
          extrema: true
          icon: false
          name: true
          state: true
          graph: line
        color_thresholds:
          - value: 0
            color: "#44739e"
          - value: 18
            color: "#1db954"
          - value: 24
            color: "#f0a202"
          - value: 28
            color: "#d00000"
      - type: custom:mini-graph-card
        name: Fan Actual (12h)
        entities: [ sensor.hrv_hrv_fan_actual ]
        hours_to_show: 12
        points_per_hour: 8
        line_width: 3
        show:
          icon: false
          name: true
          state: true
          graph: bar

  # ===== FILTER HEALTH SIDE BY SIDE =====
  - type: horizontal-stack
    cards:
      - type: custom:bar-card
        name: Life %
        entities:
          - entity: sensor.hrv_filter_life_2
            name:  Life %
        height: 24
        positions: { icon: "off", indicator: "off", title: inside, value: inside }
        min: 0
        max: 100
        severity:
          - color: "#d00000"  # 0-20
            from: 0
            to: 20
          - color: "#f0a202"  # 20-50
            from: 20
            to: 50
          - color: "#1db954"  # 50-100
            from: 50
            to: 100
      - type: custom:bar-card
        name: Days
        entities:
          - entity: sensor.hrv_hrv_filter_days
            name:  Days
        height: 24
        positions: { icon: "off", indicator: "off", title: inside, value: inside }
        min: 0
        max: 365
        severity:
          - color: "#d00000"   # 0-30
            from: 0
            to: 30
          - color: "#f0a202"   # 30-120
            from: 30
            to: 120
          - color: "#1db954"   # 120+
            from: 120
            to: 365
```

<img width="354" height="873" alt="image" src="https://github.com/user-attachments/assets/f5a4f57c-e3fa-4543-ac92-ea758efed620" />




