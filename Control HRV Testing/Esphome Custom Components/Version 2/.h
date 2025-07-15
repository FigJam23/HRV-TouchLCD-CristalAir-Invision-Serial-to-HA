#include "esphome.h"

class HRVProtocol : public Component, public UARTDevice {
 public:
  HRVProtocol(UARTComponent *parent) : UARTDevice(parent) {}

  // Public properties for direct access
  float outdoor_temp = 0.0;
  float humidity = 0.0;
  int fan_speed = 0;
  bool comm_error = false;
  std::string system_status = "Initializing";
  std::string last_error = "None";

  // Extended command system
  void send_command(uint8_t command_type, uint8_t value) {
    std::vector<uint8_t> cmd = {0x7E, 0x30, command_type, value, 0x00};
    send_frame(cmd);
  }

  // Set temperature (converts to HRV format)
  void set_temperature(float temp) {
    uint16_t temp_value = static_cast<uint16_t>(temp * 10);
    send_command(0x00, temp_value & 0xFF);
    send_command(0x01, (temp_value >> 8) & 0xFF);
  }

  // Set fan speed with validation
  void set_fan_speed(uint8_t speed) {
    if(speed > 3) {
      last_error = "Invalid speed value";
      return;
    }
    send_command(0x04, speed);
  }

  // Manual update request
  void request_update() {
    send_command(0x05, 0x01);
  }

  // Raw command interface
  void send_raw_command(const std::string &hex_cmd) {
    std::vector<uint8_t> data;
    std::string hex = hex_cmd;
    hex.erase(std::remove(hex.begin(), hex.end(), ' '), hex.end());
    
    for (size_t i = 0; i < hex.length(); i += 2) {
      std::string byte_str = hex.substr(i, 2);
      uint8_t byte = static_cast<uint8_t>(strtol(byte_str.c_str(), nullptr, 16));
      data.push_back(byte);
    }
    write_array(data);
  }

  void setup() override {
    // Initialize communication
    system_status = "Starting";
    last_error = "None";
    comm_error = false;
  }

  void loop() override {
    while (available()) {
      uint8_t c = read();
      last_activity = millis();
      
      if (c == 0x7E) {
        if (frame_position >= 4) process_frame();
        frame_position = 0;
        frame_buffer[frame_position++] = c;
      } else if (frame_position > 0) {
        if (frame_position < sizeof(frame_buffer)) {
          frame_buffer[frame_position++] = c;
        } else {
          last_error = "Frame overflow";
          frame_position = 0;
        }
      }
    }
    
    // Check communication timeout
    if (millis() - last_activity > 5000) {
      comm_error = true;
      system_status = "No communication";
    } else {
      comm_error = false;
    }
  }

 private:
  uint8_t frame_buffer[20];
  uint8_t frame_position = 0;
  unsigned long last_activity = 0;
  
  void send_frame(std::vector<uint8_t> &data) {
    uint8_t checksum = calculate_checksum(data, 1, data.size()-1);
    data.push_back(checksum);
    data.push_back(0x7E);
    write_array(data);
  }

  uint8_t calculate_checksum(std::vector<uint8_t> &data, int start, int length) {
    uint16_t sum = 0;
    for (int i = start; i < start + length; i++) {
      sum += data[i];
    }
    return 0x100 - (sum & 0xFF);
  }

  void process_frame() {
    uint8_t calc_checksum = calculate_checksum(frame_buffer, 1, frame_position - 2);
    
    if (calc_checksum == frame_buffer[frame_position - 1]) {
      // Status frame: 7E 31 01 XX 00 YY ZZ WW CS 7E
      if (frame_position >= 10 && frame_buffer[1] == 0x31 && frame_buffer[2] == 0x01) {
        outdoor_temp = frame_buffer[3] / 10.0f;
        humidity = frame_buffer[5];
        fan_speed = frame_buffer[6];
        system_status = "Operational";
      }
      // Acknowledgement frame: 7E 30 00 XX XX CS 7E
      else if (frame_position == 7 && frame_buffer[1] == 0x30 && frame_buffer[2] == 0x00) {
        system_status = "Command Accepted";
      }
      // Error frame: 7E 30 01 XX CS 7E
      else if (frame_position == 6 && frame_buffer[1] == 0x30 && frame_buffer[2] == 0x01) {
        system_status = "Error";
        last_error = "Code: " + to_string(frame_buffer[3]);
      }
    } else {
      last_error = "Checksum failed";
    }
  }
};

class HRVFan : public Component, public Fan {
 public:
  HRVFan(HRVProtocol *hrv) : hrv_(hrv) {}

  void setup() override {
    // Initialize to last known state
    auto restore = this->restore_state_();
    if (restore.has_value()) {
      restore->apply(this);
    } else {
      // Default to off
      this->state = false;
      this->speed = 0;
    }
  }

  FanTraits get_traits() override {
    return FanTraits(false, true, false, 4);
  }

  void control(const FanCall &call) override {
    if (call.get_state().has_value()) {
      this->state = *call.get_state();
      if (!this->state) this->speed = 0;
    }
    
    if (call.get_speed().has_value()) {
      this->speed = *call.get_speed();
      if (this->speed > 0) this->state = true;
    }
    
    // Apply to HRV system
    if (this->state) {
      hrv_->set_fan_speed(this->speed);
    } else {
      hrv_->set_fan_speed(0);
    }
    
    this->publish_state();
  }

 private:
  HRVProtocol *hrv_;
};
