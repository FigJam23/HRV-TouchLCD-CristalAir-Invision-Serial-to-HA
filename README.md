HRV → Home Assistant

An ESP8266/D1 Mini ESPHome configuration that fully emulates your HRV keypad and publishes all telemetry to Home Assistant via MQTT.
No physical keypad is required — fan speed, setpoint, and filter life are controlled directly from HA.

Includes state retention via MQTT, so fan speed, setpoint, and filter life restore after reboot.

https://manuals.plus/hrv/3843-classic-controller-manual
📦 Hardware

    ESP8266 (Wemos D1 Mini shown; any 3.3 V ESP8266 will work)

    TTL logic-level shifter (5 V → 3.3 V)

    1 × capacitor (for bus noise smoothing)

    Optional: buck converter if using an external 5 V supply

    Optional: DHT22/AM2302 or similar sensor for external house temp

<!-- Diagram Gallery: last image first --> <table align="center"> <tr> <td> <img src="https://github.com/user-attachments/assets/2139913b-beab-4f28-94c3-9a203223ec30" alt="Third Diagram" width="200" /> </td> <td> <img src="https://github.com/user-attachments/assets/98608253-7fee-40a3-9767-c512c4dd577b" alt="Keypad Front" width="200" /> </td> <td> <img src="https://github.com/user-attachments/assets/ac182a4e-5fb9-4af2-a0a2-11d3567e2973" alt="Wiring Diagram 3" width="200" /> </td> </tr> </table>

Components

ESP8266 (Wemos D1 Mini)
HRV Keypad bus (removed when emulating)
Logic Level Shifter (TX/RX)

Wiring Connections

5V  → Positive (5V) [Blue]
GND → Negative (GND) [Black]
RX  → LV1 ↔ HV1 → TX [Green]
TX  → LV2 ↔ HV2 → RX [White]

Notes

Use logic level shifter for safe 3.3 V ↔ 5 V communication.
Verify RX/TX orientation before powering up.

🚀 Features

    Telemetry

        Roof (remote) temperature from HRV controller

        External house temperature (DHT22 or similar)

        Control-panel setpoint (0 = off / disabled)

        Fan speed (%)

        Purge-sensor temperature

        Summer-1 (roof-fan) temperature

        Summer-2 (roof-fan) temperature

        ATU 1 damper position (%)

        ATU 2 damper position (%)

        Heat-transfer coil temperature

        HX-preheat coil temperature

        Filter life % and days remaining

    Control

        Fan speed slider — retained via MQTT after reboot

        Setpoint slider — retained via MQTT after reboot

        Filter reset button — sets to 730 days (100%)

        On/Off — quickly turn fan off or back to last speed

    Diagnostics & Raw Data

        Raw packet hex stream

        Parsed flags (raw, decimal, binary, individual bits)

        Ability to hide raw RX/TX frames and unused keypad status in ESPHome Web UI

📡 Example Packet Data

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
0x00    ← raw temp low  byte
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
0xXX    ← raw temp low  byte
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
0x00
0xXX
0xXX
0xYY
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
0x01
0x6C
0x00
0x??    ← checksum
7E      

── HX-preheat coil temp (t=0x38, 7 bytes) ──
7E    
0x38    ← t = 0x38 (HX coil)
0x00
0x00
0xC8
0x??    ← checksum
7E
