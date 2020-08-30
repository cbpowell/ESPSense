// Copyright 2020, Charles Powell

#include "esphome.h"
#include "ESPAsyncUDP.h"
#include "ArduinoJson.h"

#define RES_SIZE 400
#define REQ_SIZE 70

class ESPSensePlug {
public:
  std::string name;
  std::string mac;
  Sensor* power_id = NULL;
  Sensor* voltage_id = NULL;
  Sensor* current_id = NULL;
  
  ESPSensePlug() {}
  ESPSensePlug(std::string name, std::string mac, Sensor* pid, Sensor* vid = NULL, Sensor* cid = NULL) {
    name = name;
    mac = mac;
    power_id = pid;
    voltage_id = vid;
    current_id = cid;
  }
};

class ESPSense : public Component {
public:
  AsyncUDP udp;
  Sensor* sensor_id = NULL;
  ESPSensePlug plugs[15]; // 15 max for now
  
  ESPSense(Sensor *sid, float conf_voltage = 120.0) : Component() {
    voltage = conf_voltage;
    sensor_id = sid;
    name = App.get_name();
    mac = get_mac_address_pretty();
    
    auto plug = ESPSensePlug();
  }
  
  ESPSense(Sensor *sid, std::string config_mac, std::string config_alias, float conf_voltage = 120.0) : Component() {
    voltage = conf_voltage;
    sensor_id = sid;
    name = config_alias;
    mac = config_mac;
  }
  
  void setup() override {
    if(udp.listen(9999)) {
      ESP_LOGI("ESPSense","Listening on port 9999");
      // Parse incoming packets
      start_sense_response();
    } else {
      ESP_LOGE("ESPSense", "Failed to start UDP listen!");
    }
  }
  
private:
  float voltage;
  char response_buf[RES_SIZE];
  
  StaticJsonBuffer<200> jsonBuffer;
  std::string name;
  std::string mac;
  
  std::string base_json = "{\"emeter\": {\"get_realtime\":{ "
                              "\"current\": %.02f, \"voltage\": %.02f, \"power\": %.02f, \"total\": 0, \"err_code\": 0}}, "
                           "\"system\": {\"get_sysinfo\": "
                              "{\"err_code\": 0, \"hw_ver\": 1.0, \"type\": \"IOT.SMARTPLUGSWITCH\", \"model\": \"HS110(US)\", "
                           "\"mac\": \"%s\", \"deviceId\": \"%s\", \"alias\": \"%s\", \"relay_state\": 1, \"updating\": 0 }}}";
  
  
  void start_sense_response() {
    udp.onPacket([&](AsyncUDPPacket &packet) {parse_packet(packet);});
  }
  
  void parse_packet(AsyncUDPPacket &packet) {
    ESP_LOGD("ESPSense", "Got packet from %s", packet.remoteIP().toString().c_str());
    
    if(packet.length() > REQ_SIZE) {
      // Not a Sense request packet
      ESP_LOGD("ESPSense", "Packet is oversized, ignoring");
      return;
    }
    
    char request_buf[REQ_SIZE];
    
    // Decrypt
    decrypt(packet.data(), packet.length(), request_buf);
    
    // Add null terminator
    request_buf[packet.length()] = '\0';
    
    // Print into null-terminated string
    ESP_LOGD("ESPSense", "Got message: %s", request_buf);
    
    // Parse JSON
    jsonBuffer.clear();
    JsonObject &req = jsonBuffer.parseObject(request_buf);
    if(!req.success()) {
      ESP_LOGW("ESPSense", "JSON parse failed!");
    }
    
    // Check if this is a valid request by looking for emeter key
    JsonVariant request = req["emeter"]["get_realtime"];
    if (request.success()) {
      ESP_LOGD("ESPSense", "Power measurement requested");
      // Generate JSON response string
      int response_len = generate_response(response_buf);
      // Encrypt
      char encrypted[response_len];
      encrypt(response_buf, response_len, encrypted);
      // Respond to request
      packet.write((uint8_t *)encrypted, response_len);
    }
  }
  
  void decrypt(const uint8_t* data, size_t len, char* result) {
    uint8_t key = 171;
    uint8_t a;
    for (int i = 0; i < len; i++) {
      uint8_t unt = data[i];
      a = unt ^ key;
      key = unt;
      result[i] = char(a);
    }
  }
  
  void encrypt(const char* data, size_t len, char* result) {
    uint8_t key = 171;
    uint8_t a;
    for (int i = 0; i < len; i++) {
      uint8_t unt = data[i];
      a = unt ^ key;
      key = a;
      result[i] = a;
    }
  }
  
  float get_power() {
    float state;
    if(sensor_id && id(sensor_id).has_state()) {
      state = id(sensor_id).state;
    } else {
      state = 0.0;
    }
    return state;
  }
  
  float get_voltage() {
    return voltage;
  }
  
  int generate_response(char* data) {
    float power = get_power();
    float voltage = get_voltage();
    float current = power / voltage;
    int response_len = snprintf(data, RES_SIZE, base_json.c_str(), current, voltage, power, mac.c_str(), mac.c_str(), name.c_str());
    ESP_LOGD("ESPSense", "JSON out: %s", data);
    return response_len;
  }
};