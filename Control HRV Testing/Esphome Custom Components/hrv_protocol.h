#include "esphome.h"

class HRVProtocol : public Component, public UARTDevice {
 public:
  HRVProtocol(UARTComponent *parent) : UARTDevice(parent) {}

  float get_outdoor_temp() { return outdoor_temp; }
  std::string get_system_status() { return system_status; }
  
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

  void setup() override {
    // Initialization if needed
  }

  void loop() override {
    while (available()) {
      uint8_t c = read();
      
      if (c == 0x7E) {
        if (frame_position > 2) process_frame();
        frame_position = 0;
      }
      
      if (frame_position < sizeof(frame_buffer)) {
        frame_buffer[frame_position++] = c;
      }
    }
  }

 private:
  uint8_t frame_buffer[20];
  uint8_t frame_position = 0;
  float outdoor_temp = 0.0;
  std::string system_status = "Unknown";
  
  uint8_t calculate_checksum(std::vector<uint8_t> &data, int start, int length) {
    uint16_t sum = 0;
    for (int i = start; i < start + length; i++) {
      sum += data[i];
    }
    return 0x100 - (sum & 0xFF);
  }

  void process_frame() {
    // Validate checksum first
    uint16_t sum = 0;
    for (int i = 0; i < frame_position - 2; i++) {
      sum += frame_buffer[i];
    }
    uint8_t calc_checksum = 0x100 - (sum & 0xFF);
    
    if (calc_checksum == frame_buffer[frame_position-2]) {
      // HRV Status Frame: 7E 31 01 XX 00 YY ZZ WW CS 7E
      if (frame_position >= 10 && frame_buffer[0] == 0x31 && frame_buffer[1] == 0x01) {
        // Byte 3: Outdoor temp (hex to °C: 0x56 = 86 = 8.6°C)
        outdoor_temp = frame_buffer[3] / 10.0f;
        
        // Byte 5: Humidity (0x1E = 30%)
        // Byte 6: Fan speed (0x04 = speed 4? May need mapping)
        
        // Update system status
        system_status = "Operational";
      }
      // Acknowledgement Frame: 7E 30 00 XX XX CS 7E
      else if (frame_position == 7 && frame_buffer[0] == 0x30 && frame_buffer[1] == 0x00) {
        system_status = "Command Accepted";
      }
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
