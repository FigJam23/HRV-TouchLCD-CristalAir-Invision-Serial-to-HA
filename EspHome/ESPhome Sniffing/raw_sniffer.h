#pragma once
#include "esphome.h"
#include <ESP8266WebServer.h>

class RawSniffer : public Component {
 public:
  RawSniffer() : server_(80) {}

  void setup() override {
    server_.on("/", [this]() {
      // Auto-refresh the page every 1 second
      std::string html = "<!DOCTYPE html><html><head>"
                         "<meta charset='utf-8'><meta http-equiv='refresh' content='10'>"
                         "<title>HRV Sniffer</title>"
                         "</head><body>"
                         "<h3>Frame A (7E 31 01 …)</h3><pre>" + id(frame_a_buf) + "</pre>"
                         "<h3>Frame B (7E 30 00 …)</h3><pre>" + id(frame_b_buf) + "</pre>"
                         "<h3>Unknown</h3><pre>" + id(frame_unknown_buf) + "</pre>"
                         "</body></html>";
      server_.send(200, "text/html", html.c_str());
    });
    server_.begin();
    ESP_LOGI("raw_sniffer", "HTTP server started");
  }

  void loop() override {
    server_.handleClient();
  }

 private:
  ESP8266WebServer server_;
};
