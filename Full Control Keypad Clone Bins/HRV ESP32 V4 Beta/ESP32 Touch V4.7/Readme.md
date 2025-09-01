# Bugs Fix's and added return to home screen time outs

# Demo of this Version

https://youtube.com/shorts/ByNAx1_QtX8?si=mW8x7UHjuPzPWhZI


<img width="930" height="278" alt="image" src="https://github.com/user-attachments/assets/fbc4d95a-bf50-4a74-8571-9cf65c2a7914" />


# Hassio Card

Requirements for HRV – Mobile Dashboard Card

This Lovelace configuration uses several custom cards from HACS (Home Assistant Community Store).
Make sure you have the following installed:

Core UI

Mushroom Cards

mushroom-title-card

mushroom-chips-card

mushroom-template-card

mushroom-number-card

mushroom-entity-card



Layout / Styling

stack-in-card

Used to group controls (Set °C, Fan %, Boost, Power) in a clean container.



Graphs / Visualization

mini-graph-card

For temperature and fan history charts.




---

Entities Used

The card assumes you have these entities available (adjust to your setup):

House sensors

sensor.hrv_house_temp_2 (°C)

sensor.hrv_house_humidity_2 (%RH)


Roof/attic sensors

sensor.hrv_hrv_roof_temp (°C)

sensor.floating_unit_roof_living_humidity (%RH)


HRV controls

switch.hrv_hrv_power

switch.hrv_hrv_boost

binary_sensor.hrv_boost_active_2

sensor.hrv_hrv_boost_remaining

number.hrv_hrv_setpoint

number.hrv_hrv_fan

sensor.hrv_hrv_fan_actual


Filter status

binary_sensor.hrv_filter_replacement_needed_2




---

Features

House & Roof environment cards: Temp + Humidity with comfort labels (Dry / Comfort / Humid).

Boost button: Displays remaining timer when active, tap to toggle.

Power button: Shows On/Off and embeds Filter status (badge + subtitle).

Controls: Sliders for Setpoint and Fan %.

Graphs:

24h House vs Roof Temp comparison.

12h Fan Actual history.

<img width="649" height="752" alt="image" src="https://github.com/user-attachments/assets/0bfebeaa-d236-4148-a9df-6842acefcf2a" />


```
type: vertical-stack
cards:
  - type: custom:mushroom-title-card
    title: HRV – Mobile
    title_tap_action: { action: url, url_path: http://192.168.1.103 }

  # House & Roof tiles
  - type: horizontal-stack
    cards:
      - type: custom:mushroom-template-card
        entity: sensor.hrv_house_temp_2
        icon: mdi:home-thermometer
        icon_color: >
          {% set t = states('sensor.hrv_house_temp_2')|float(0) %}
          {% if t < 15 %}blue{% elif t < 22 %}green{% elif t < 27 %}amber{% else %}red{% endif %}
        primary: >
          House {{ states('sensor.hrv_house_temp_2') }}°C · {{ states('sensor.hrv_house_humidity_2') }}%
        secondary: >
          {% set h = states('sensor.hrv_house_humidity_2')|float(0) %}
          {% if h < 35 %}Dry{% elif h <= 60 %}Comfort{% else %}Humid{% endif %} • Indoor
        layout: vertical
        fill_container: true
        tap_action: { action: more-info }

      - type: custom:mushroom-template-card
        entity: sensor.hrv_hrv_roof_temp
        icon: mdi:home-roof
        icon_color: >
          {% set t = states('sensor.hrv_hrv_roof_temp')|float(0) %}
          {% if t < 10 %}blue{% elif t < 25 %}green{% elif t < 35 %}amber{% else %}red{% endif %}
        primary: >
          Roof {{ states('sensor.hrv_hrv_roof_temp') }}°C · {{ states('sensor.floating_unit_roof_living_humidity') }}%
        secondary: >
          {% set h = states('sensor.floating_unit_roof_living_humidity')|float(0) %}
          {% if h < 35 %}Dry{% elif h <= 60 %}Comfort{% else %}Humid{% endif %} • Attic
        layout: vertical
        fill_container: true
        tap_action: { action: more-info }

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
          # BOOST button (shows timer when active)
          - type: custom:mushroom-template-card
            entity: switch.hrv_hrv_boost
            icon: mdi:rocket-launch
            icon_color: >
              {% if is_state('binary_sensor.hrv_boost_active_2','on') %}purple{% else %}grey{% endif %}
            primary: Boost
            secondary: >
              {% if is_state('binary_sensor.hrv_boost_active_2','on') %}
                {{ states('sensor.hrv_hrv_boost_remaining') }}
              {% else %}Off{% endif %}
            layout: vertical
            fill_container: true
            tap_action:
              action: call-service
              service: switch.toggle
              target: { entity_id: switch.hrv_hrv_boost }
            hold_action: { action: more-info }

          # POWER button with FILTER integrated (badge + subtitle)
          - type: custom:mushroom-template-card
            entity: switch.hrv_hrv_power
            icon: mdi:power
            icon_color: >
              {% if is_state('switch.hrv_hrv_power','on') %}green{% else %}grey{% endif %}
            primary: Power
            secondary: >
              {% if is_state('switch.hrv_hrv_power','on') %}On{% else %}Off{% endif %}
              ·
              {% if is_state('binary_sensor.hrv_filter_replacement_needed_2','on') %}
                Filter Needed
              {% else %}Filter OK{% endif %}
            badge_icon: mdi:air-filter
            badge_color: >
              {% if is_state('binary_sensor.hrv_filter_replacement_needed_2','on') %}red{% else %}green{% endif %}
            layout: vertical
            fill_container: true
            tap_action: { action: toggle }
            hold_action: { action: more-info }

  # Graphs
  - type: horizontal-stack
    cards:
      - type: custom:mini-graph-card
        name: Temps (24h)
        entities:
          - entity: sensor.hrv_house_temp_2
            name: House
          - entity: sensor.hrv_hrv_roof_temp
            name: Roof
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
          - { value: 0,  color: "#44739e" }
          - { value: 18, color: "#1db954" }
          - { value: 24, color: "#f0a202" }
          - { value: 28, color: "#d00000" }

      - type: custom:mini-graph-card
        name: Fan Actual (12h)
        entities: [sensor.hrv_hrv_fan_actual]
        hours_to_show: 12
        points_per_hour: 8
        line_width: 3
        show:
          icon: false
          name: true
          state: true
          graph: bar
```
