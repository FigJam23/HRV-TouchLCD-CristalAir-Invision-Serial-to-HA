#include "esphome.h"

class HRVProtocol : public Component, public UARTDevice {
 public:
  HRVProtocol(UARTComponent *parent) : UARTDevice(parent) {}

  // Public properties for sensor values
  float outdoor_temp = 0.0;
  float humidity = 0.0;
  int fan_speed = 0;
  std::string system_status = "Unknown";
  
  void set_temperature(float temp) {
    uint16_t temp_value = static_cast<uint16_t>(temp * 10);
    uint8_t low_byte = temp_value & 0xFF;
    uint8_t high_byte = (temp_value >> 8) & 0xFF;
    
    std::vector<uint8_t> cmd = {0x7E, 0x30, 0x00, low_byte, high_byte};
    uint8_t checksum = calculate_checksum(cmd, 2, cmd.size()-2);
    cmd.push_back(checksum);
    cmd.push_back(0x7E);
    
    write_array(cmd);
  }

  void set_fan_speed(uint8_t speed) {
    // Speed: 0=Off, 1=Low, 2=Medium, 3=High
    std::vector<uint8_t> cmd = {0x7E, 0x30, 0x01, speed, 0x00};
    uint8_t checksum = calculate_checksum(cmd, 2, cmd.size()-2);
    cmd.push_back(checksum);
    cmd.push_back(0x7E);
    
    write_array(cmd);
  }

  void send_raw_command(const std::string &hex_command) {
    // Convert hex string to bytes (hex_command is a string of hex bytes, e.g., "7E 30 00 C2 00 0E 7E")
    std::vector<uint8_t> data;
    std::string hex = hex_command;
    // Remove spaces if any
    hex.erase(std::remove(hex.begin(), hex.end(), ' '), hex.end());
    for (size_t i = 0; i < hex.length(); i += 2) {
      auto byte_str = hex.substr(i, 2);
      uint8_t byte = (uint8_t) strtol(byte_str.c_str(), NULL, 16);
      data.push_back(byte);
    }
    write_array(data);
  }

  void setup() override {
    // Initialization if needed
  }

  void loop() override {
    while (available()) {
      uint8_t c = read();
      
      if (c == 0x7E) {
        if (frame_position > 0) { // We have a frame to process
          process_frame();
        }
        frame_position = 0;
        frame_buffer[frame_position++] = c; // Start new frame with 0x7E
      } else if (frame_position > 0) {
        if (frame_position < sizeof(frame_buffer)) {
          frame_buffer[frame_position++] = c;
        } else {
          // Buffer overflow, reset
          frame_position = 0;
        }
      }
    }
  }

 private:
  uint8_t frame_buffer[20];
  uint8_t frame_position = 0;
  
  uint8_t calculate_checksum(std::vector<uint8_t> &data, int start, int length) {
    uint16_t sum = 0;
    for (int i = start; i < start + length; i++) {
      sum += data[i];
    }
    return 0x100 - (sum & 0xFF);
  }

  void process_frame() {
    // We have a frame from index 0 to frame_position-1
    // Validate checksum: the second last byte is the checksum (last is 0x7E, which we haven't stored in the buffer for the end of frame)
    // But note: our frame includes the starting 0x7E and the data, but the ending 0x7E is not in the buffer (because it triggered the end)
    // So the frame structure: [0x7E, ... , checksum] and then we got a new 0x7E which ended the frame.
    // Therefore, the last byte in our buffer is the checksum? Actually, the frame ends with 0x7E, so the byte before the last 0x7E is the checksum.
    // But we didn't store the ending 0x7E. So our frame_buffer has [0x7E, ... , last_data_byte, checksum] and then we saw the next 0x7E.

    // The frame must be at least 4 bytes: [0x7E, ADDR, CTL, ... , CS]?
    if (frame_position < 4) {
      // Too short, ignore
      return;
    }

    // The checksum is the byte at frame_position-1 (the last byte before the ending 0x7E that triggered the end)
    uint8_t received_checksum = frame_buffer[frame_position-1];
    // Calculate checksum over the frame except the starting 0x7E and the checksum itself (from index 1 to frame_position-2)
    uint16_t sum = 0;
    for (int i = 1; i < frame_position-1; i++) {
      sum += frame_buffer[i];
    }
    uint8_t calc_checksum = 0x100 - (sum & 0xFF);

    if (calc_checksum == received_checksum) {
      // Checksum valid
      // Check for known frame types
      // HRV Status Frame: 7E 31 01 XX 00 YY ZZ WW CS
      if (frame_buffer[1] == 0x31 && frame_buffer[2] == 0x01 && frame_position >= 10) {
        // Byte 3: Outdoor temp (e.g., 0x56 = 86 = 8.6°C)
        outdoor_temp = frame_buffer[3] / 10.0f;
        // Byte 5: Humidity (0x1E = 30%)
        humidity = frame_buffer[5];
        // Byte 6: Fan speed (0x04 = speed 4? But note: in the command we set 0-3, so we may need to map)
        fan_speed = frame_buffer[6]; // This gives the raw value, might need mapping to 0-3
        system_status = "Operational";
      }
      // Acknowledgement Frame: 7E 30 00 XX XX CS
      else if (frame_buffer[1] == 0x30 && frame_buffer[2] == 0x00 && frame_position == 7) {
        system_status = "Command Accepted";
      }
    } else {
      // Checksum failed
      system_status = "Checksum Error";
    }
  }
};

class HRVFan : public Component, public Fan {
 public:
  HRVFan(HRVProtocol *hrv) : hrv_(hrv) {}

  void setup() override {
    // Restore previous state if needed
  }

  FanTraits get_traits() override {
    return FanTraits(false, true, true, 4);  // Oscillation, speed, direction, speed count
  }

  void control(const FanCall &call) override {
    if (call.get_state().has_value()) {
      // Turn on/off
      is_on = *call.get_state();
      set_speed_(current_speed);
    }
    if (call.get_speed().has_value()) {
      // Set speed
      current_speed = *call.get_speed();
      set_speed_(current_speed);
    }
    publish_state();
  }

 private:
  HRVProtocol *hrv_;
  bool is_on = false;
  int current_speed = 0;
  
  void set_speed_(int speed) {
    if (!is_on) speed = 0;
    hrv_->set_fan_speed(speed);
  }
};
