
<img width="314" height="901" alt="image" src="https://github.com/user-attachments/assets/0526cf6b-d4c7-4d6a-b1d2-36b9ce599021" />
Jason Cap Example

````
{
  "profile_name": "HRV learned profile",
  "format": "payload_only_no_7E_no_checksum",
  "baud": 1200,
  "serial": "8N1",
  "checksum": "negative_sum_payload_bytes",
  "notes": "Payloads only. Firmware should wrap with 7E and calculate checksum.",
  "startup_sequence": [],
  "fan_mode": "percent_template",
  "fan_template": {"prefix":"31 01","low_code":"4E","medium_code":"4F","high_code":"50","percent_index":3,"suffix":"1E 84 F0","example_low_percent":0,"example_medium_percent":51,"example_high_percent":100},
  "fan": {
    "low": "31 01 4E 00 1E 84 F0",
    "medium": "31 01 4F 33 1E 84 F0",
    "high": "31 01 50 64 1E 84 F0",
    "off": "31 01 4E 00 1E 84 F0",
    "on": "31 01 4E 1E 1E 84 F0"
  },
  "fan_mode_notes": {
    "percent_template": "Use prefix + speed code + requested percent byte + suffix for true 0-100 control.",
    "fixed_frame": "Replay learned low/medium/high payloads. Slider should map to speed bands.",
    "incomplete": "Learn low, medium, and high before importing into controller firmware."
  },
  "learned_candidates": {
    "last_action": "fan_on",
    "last_candidates": [{"payload":"31 01 50 64 1E 84 F0","count":1},{"payload":"31 01 4E 1E 1E 84 F0","count":2}]
  },
  "stats": {
    "valid_frames": 159,
    "invalid_frames": 0,
    "last_frame_ms": 118704
  }
}

````
