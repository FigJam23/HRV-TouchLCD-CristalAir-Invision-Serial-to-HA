
HRV Keypad Beta 4.8

Beta 4.8 is a testing build focused on reliability, state syncing, and better recovery after power loss or network interruptions. This version improves how the touchscreen UI, web UI, MQTT, and retained settings stay aligned, while also adding SD card persistence for key values and status history.

What is new in Beta 4.8
Power state syncing improvements

The power state now behaves much more consistently across the touchscreen, web UI, and Home Assistant/MQTT.

Changes include:

Added dedicated MQTT power state and command topics

Added Home Assistant discovery for HRV Power

Power state is now restored from saved settings on boot

Touchscreen power button now reflects the saved power state at startup

Web UI power button text and color now update live

When powered off, the UI shows fan speed as 0 instead of stale values

Power off now disables fan slider, boost button, and setpoint control

Power on restores the correct running state instead of leaving the UI out of sync

Better boot and restart behaviour

This version improves startup handling after reboots and power cuts.

Changes include:

Saved power state is loaded from NVS on boot

Saved target fan speed and last non-zero fan speed are restored correctly

Web UI and touchscreen now initialise from actual saved state

Added reboot endpoint in web UI

Added touchscreen reboot button support with tap info and long-press reboot

Added Home Assistant availability topic for online/offline status

Burnt Toast mode improvements

Burnt Toast mode now behaves more cleanly and visibly.

Changes include:

Added visible on-screen countdown timer

Countdown text turns red while active

Remaining time is published to MQTT

Total boost duration is published to MQTT

Burnt Toast state is correctly cleared if power is switched off

Burnt Toast button state now syncs better with internal logic

Web UI improvements

The browser UI has been expanded and cleaned up.

Changes include:

Live power button text and color state

Timer settings page for:

Burnt Toast duration

Manual override hold time

Sensor override page for inbound MQTT override topics

MQTT broker configuration page

Wi-Fi configuration page

OTA firmware update page

Reboot control page

Better live status refresh for fan, boost, temperatures, and mode

MQTT and Home Assistant improvements

MQTT support has been expanded significantly for better integration.

Added:

Power state topic

Power command topic

Boost command topic

Boost remaining seconds topic

Boost total seconds topic

Filter days set topic

Retained setpoint state

Retained power state

Availability topic

Control debug topic

Home Assistant discovery now includes:

House temperature

House humidity

Roof temperature

Roof humidity

Fan percent

Filter days

Filter life

Filter replacement needed

Boost active

Boost remaining

Power switch

Boost switch

Setpoint number

Filter days number

SD card persistence and logging

Beta 4.8 adds SD card support for persistence and simple status logging.

Added:

/hrv/setpoint.json

/hrv/filter.json

/hrv/status.json

Functions include:

Restore setpoint from SD card on boot

Restore filter days from SD card on boot

Sync filter day changes to SD card

Debounced setpoint save to SD after changes

Hourly status snapshot logging

Status log includes:

timestamp

local datetime

uptime

setpoint

filter days remaining

Filter life handling improvements

Filter tracking is now more complete.

Changes include:

Filter days can be changed from MQTT/Home Assistant

Filter days are saved to NVS

Filter days are synced to SD card

Daily decrement now persists correctly

Reset button restores filter days to full life

Filter status updates retained MQTT topics properly

Sensor handling improvements

Sensor input logic is now a bit more flexible and robust.

Changes include:

Supports inbound MQTT override topics for:

house temp

house humidity

roof temp

roof humidity

Override values are treated as fresh for a short time window

SHT31 and HRV UART readings no longer immediately overwrite fresh MQTT override values

Initial SHT31 sample is taken at boot to seed UI and MQTT faster

Roof probe logic continues until valid roof temperature frame is received

UI quality improvements

General touchscreen behaviour has been improved.

Changes include:

Power button reflection function added

Fan slider reflection improved

Off state now forces UI fan display to 0

Auto return to Home screen after inactivity

Better icon refresh timing

Improved reboot info display

Beep feedback on more UI actions

Important Beta Notes

This is still a beta build and needs more testing.

Areas still needing confirmation:

Touchscreen power button style/state logic on all themes/layout exports

State recovery after full power cuts

Correct restore behaviour when Wi-Fi is unavailable at boot

MQTT retained state behaviour after broker restarts

Burnt Toast countdown/state edge cases

SD card behaviour with missing or slow cards

Manual override timing interactions with auto mode

Actual HRV fan state vs displayed state during startup transitions

Known testing focus for Beta 4.8

Recommended things to test:

Turn HRV off from touchscreen, then reboot device

Turn HRV off from Home Assistant, then cut power and restore power

Turn HRV off from web UI and confirm touchscreen updates

Start Burnt Toast, then power off

Change filter days from Home Assistant and confirm SD + UI update

Change setpoint and confirm delayed SD write

Boot without Wi-Fi and confirm AP fallback works

Reconnect Wi-Fi and confirm MQTT reconnects properly

Confirm web button colors and labels match actual state

Confirm touchscreen red power state matches OFF state

Version label

Latest beta: 4.8
Previous stable/reference version: 4.7
