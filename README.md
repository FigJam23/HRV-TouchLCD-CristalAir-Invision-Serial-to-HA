# HRV → Home Assistant

A simple ESP8266/D1 Mini sketch that reads TTL-serial data from your HRV control panel and publishes it to Home Assistant via MQTT. Pulls house & roof temperatures, fan speed and control-panel setpoint; plans for sending control commands in a future update.

---

## 📦 Hardware

- ESP8266 (D1 Mini shown; any 3.3 V ESP8266 will work)  
- TTL logic-level shifter (5 V → 3.3 V)   
- 1 cap
- Optional: buck converter if you switch to an external 5 V supply




<!-- Row of four images -->
<div style="
  display: flex;
  flex-wrap: wrap;
  justify-content: center;
  align-items: flex-start;
  gap: 0.5em;
">
  <img
    style="width: 23%; height: auto;"
    alt="Diagram 1"
    src="https://github.com/user-attachments/assets/18cdaeea-f2a3-4aa9-b0f8-0c1edc792d2e"
  />
  <img
    style="width: 23%; height: auto;"
    alt="Diagram 2"
    src="https://github.com/user-attachments/assets/ccee0cb8-b140-4ca7-a427-a638f3edf10d"
  />
  <img
    style="width: 23%; height: auto;"
    alt="Diagram 3"
    src="https://github.com/user-attachments/assets/ac182a4e-5fb9-4af2-a0a2-11d3567e2973"
  />
  <img
    style="width: 23%; height: auto;"
    alt="Diagram 4"
    src="https://github.com/user-attachments/assets/97caabfe-2dc7-4d4e-9f41-9ce7b2998c1b"
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
  **Multiple Controller Support**
  - House temperature  
  - Roof (remote) temperature
  - Control-panel setpoint  
  - Fan speed
  - Humidity  
  - Purge-sensor temperature  
  - Summer-1 (roof-fan) temperature  
  - Summer-2 (roof-fan) temperature  
  - ATU 1 damper position (%)  
  - ATU 2 damper position (%)  
  - Heat-transfer coil temperature  
  - HX-preheat coil temperature  

- **Diagnostics & Raw Data**  
  - Raw packet hex stream  
  - Parsed flags (raw, decimal, binary, individual bits)
```
  

```

── Roof temp (t=0x30, 7 bytes) ──
7E      ← Start
0x30    ← t = 0x30 (roof/remote temp)
0x00    ← sensor-ID = 0 (roof)
0xCF    ← raw temp high byte
0x00    ← raw temp low  byte   (raw = 0x00CF → 207 × 0.0625 = 12.9 °C)
0x01    ← checksum
7E      ← End

── House temp + fan + setpoint (t=0x31, 10 bytes) ──
7E      ← Start
0x31    ← t = 0x31 (house/keypad)
0x01    ← sensor-ID = 1 (house)
0x6D    ← raw temp high byte
0x00    ← raw temp low  byte   (raw = 0x6D00 → 27904 × 0.0625 = 1744 °C*?)  
           *only bytes 3–4 form temp; low nibble often zero 
0x1E    ← fan speed = 0x1E (30 %)
0x14    ← setpoint = 0x14 (20 °C)
0x84    ← fixed payload
0x70    ← fixed payload
0x47    ← checksum
7E      ← End

── Purge sensor temp (t=0x32, 7 bytes) ──
7E    
0x32    ← t = 0x32 (purge temp)
0x00    ← sensor-ID = 0 (roof/purge)
0xXX    ← raw temp high byte
0xXX    ← raw temp low  byte   (× 0.0625 °C)
0xYY    ← checksum
7E      

── Summer-1 roof-fan temp (t=0x33, 7 bytes) ──
7E    
0x33    ← t = 0x33 (summer 1)
0x00    ← sensor-ID = 0
0xXX    ← raw temp high byte
0xXX    ← raw temp low  byte
0xYY    ← checksum
7E      

── Summer-2 roof-fan temp (t=0x34, 7 bytes) ──
7E    
0x34    ← t = 0x34 (summer 2)
0x00    ← sensor-ID = 0
0xXX
0xXX
0xYY
7E      

── ATU 1 damper % (t=0x35, 7 bytes) ──
7E    
0x35    ← t = 0x35 (ATU 1)
0x00    ← sensor-ID = 0
0xXX    ← high byte (0x00 usually)
0xXX    ← low byte  (→ % open)
0xYY    ← checksum
7E      

── ATU 2 damper % (t=0x36, 7 bytes) ──
7E    
0x36    ← t = 0x36 (ATU 2)
0x00
0xXX
0xXX
0xYY
7E      

── Heat-transfer coil temp (t=0x37, 7 bytes) ──
7E    
0x37    ← t = 0x37 (coil temp)
0x01    ← sensor-ID = 1 (coil)
0x6C    ← raw temp high byte
0x00    ← raw temp low  byte   (raw = 0x6C00)
0x??    ← checksum
7E      

── HX-preheat coil temp (t=0x38, 7 bytes) ──
7E    
0x38    ← t = 0x38 (HX coil)
0x00    ← sensor-ID = 0
0x00    ← high byte
0xC8    ← low byte (200 × 0.0625 = 12.5 °C)
0x??    ← checksum
7E
```
