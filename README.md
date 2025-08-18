##### Demo Keyoad Replacement ######
```
Latest Version

```
https://youtu.be/KD8OSI9nA7g
```
Latest Version

```
https://youtube.com/shorts/XVz4phSSpbA?si=MpAsYGo4Ojgp8Aac

https://youtube.com/shorts/8ISQGKKpurk?si=fGekwMP1LDX2R__V
##### Demo Keyoad Replacement ######

HRV → Home Assistant

An ESP8266/D1 Mini ESPHome configuration that fully emulates your HRV keypad and publishes all telemetry to Home Assistant via MQTT.
No physical keypad is required — fan speed, setpoint, and filter life are controlled directly from HA.

Includes state retention via MQTT, so fan speed, setpoint, and filter life restore after reboot.

This project will be moving to a ESP32 device due to limitations of memory of the esp8266 chipset, with touch screen and custome U.I's Themes etc. End Goal will a full Replacement Keypad with Touch screen or remote control via web access or hassio.


https://manuals.plus/hrv/3843-classic-controller-manual
📦 Hardware

    ESP8266 (Wemos D1 Mini shown; any 3.3 V ESP8266 will work)

    TTL logic-level shifter (5 V → 3.3 V)

    1 × capacitor (for bus noise )

    Optional: DHT22/AM2302 or similar sensor for external house temp
<!-- New Keypad Build ESP32 - Image Gallery -->



<table align="center" style="border-collapse: collapse;">
  <tr>
    <td align="center" style="padding: 10px;">
      <img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Images/4.jpg" width="200"/><br/>
      <sub>Keypad 1</sub>
    </td>
  <tr>
    <td align="center" style="padding: 10px;">
      <img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Images/5.jpg" width="200"/><br/>
      <sub>Keypad 1</sub>
    </td>
    <td align="center" style="padding: 10px;">
      <img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Images/1.jpg" width="200"/><br/>
      <sub>Keypad 1</sub>
    </td>
  <tr>
    <td align="center" style="padding: 10px;">
      <img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/6e0b0809-e17d-45b1-8727-2efd2609b403.png" width="200"/><br/>
      <sub>Keypad 1</sub>
    </td>
    <td align="center" style="padding: 10px;">
      <img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/file_0000000086a461f8abbdfd0f251aac8b(1).png" width="200"/><br/>
      <sub>Keypad 2</sub>
    </td>
    <td align="center" style="padding: 10px;">
      <img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/file_00000000bd6c622f9eaf868b7891e35b.png" width="200"/><br/>
      <sub>Keypad 3</sub>
    </td>
  </tr>
  <tr>
    <td align="center" style="padding: 10px;">
      <img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/file_0000000060ec61fd84d62c2a60668eef.png" width="200"/><br/>
      <sub>Keypad 4</sub>
    </td>
    <td align="center" style="padding: 10px;">
      <img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/f7614291-c459-4cea-b07e-f4bfe5d946c0(4).png" width="200"/><br/>
      <sub>Keypad 5</sub>
    </td>
    <td align="center" style="padding: 10px;">
      <img src="https://raw.githubusercontent.com/FigJam23/HRV-TouchLCD-CristalAir-Invision-Serial-to-HA/main/Arduino%20Keypad%20Clone%20ESP32/Keypad%20Home%20Screens/file_00000000ba5061f8b88bcc3f6a8952d6.png" width="200"/><br/>
      <sub>Keypad 6</sub>
    </td>
  </tr>
  <tr>
    <td align="center" style="padding: 10px;">
      <img src="https://github.com/user-attachments/assets/2139913b-beab-4f28-94c3-9a203223ec30" width="200"/><br/>
      <sub>Hassio 1</sub>
    </td>
    <td align="center" style="padding: 10px;">
      <img src="https://github.com/user-attachments/assets/98608253-7fee-40a3-9767-c512c4dd577b" width="200"/><br/>
      <sub>Hassio 2</sub>
    </td>
  </tr>
</table>


Components

ESP8266 (Wemos D1 Mini)
HRV Keypad bus (removed when emulating)
Logic Level Shifter (D1/D2 or TX/RX)

Wiring Connections

5V  → Positive (5V) [Blue]

GND → Negative (GND) [Black]

RX  → LV1 ↔ HV1 → TX/RX [Green]

TX  → LV2 ↔ HV2 → RX/RX [White]

Notes

Use logic level shifter for safe 3.3 V ↔ 5 V communication.

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
