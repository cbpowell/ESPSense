// Copyright 2020, Charles Powell

#include "esphome.h"
#include "ESPAsyncUDP.h"
#include "ArduinoJson.h"

#define RES_SIZE 400
#define REQ_SIZE 70
#define MAX_PLUG_COUNT 10  // Somewhat arbitrary as of now

class ESPSensePlug {
public:
  std::string name;
  std::string mac;
  float voltage = 120.0;
  Sensor *power_sid = NULL;
  Sensor *voltage_sid = NULL;
  Sensor *current_sid = NULL;
  
  std::string base_json = "{\"emeter\": {\"get_realtime\":{ "
                              "\"current\": %.02f, \"voltage\": %.02f, \"power\": %.02f, \"total\": 0, \"err_code\": 0}}, "
                           "\"system\": {\"get_sysinfo\": "
                              "{\"err_code\": 0, \"hw_ver\": 1.0, \"type\": \"IOT.SMARTPLUGSWITCH\", \"model\": \"HS110(US)\", "
                           "\"mac\": \"%s\", \"deviceId\": \"%s\", \"alias\": \"%s\", \"relay_state\": 1, \"updating\": 0 }}}";
  
  
  ESPSensePlug(Sensor *sid, std::string config_mac, std::string config_name, float config_voltage) {
    power_sid = sid;
    mac = config_mac;
    name = config_name;
    voltage = config_voltage;
  }
  
  float get_power() {
    return get_sensor_reading(power_sid, 0.0);
  }
  
  float get_voltage() {
    return get_sensor_reading(voltage_sid, voltage);
  }
  
  float get_current() {
    return get_sensor_reading(current_sid, get_power() / get_voltage());
  }
  
  float get_sensor_reading(Sensor *sid, float default_value) {
    if(sid != NULL && id(sid).has_state()) {
      return id(sid).state;
    } else {
      return default_value;
    }
  }
  
  int generate_response(char *data) {
    float power = get_power();
    float voltage = get_voltage();
    float current = get_current();
    int response_len = snprintf(data, RES_SIZE, base_json.c_str(), current, voltage, power, mac.c_str(), mac.c_str(), name.c_str());
    ESP_LOGD("ESPSense", "JSON out: %s", data);
    return response_len;
  }
};

class ESPSense : public Component {
public:
  AsyncUDP udp;
  Sensor* sensor_id = NULL;
  
  ESPSense() : Component() {}
  
  ESPSense(Sensor *sid, float conf_voltage = 120.0) : Component() {
    // Generate single plug
    std::string name = App.get_name();
    std::string mac = get_mac_address_pretty();
    
    ESPSensePlug plug = ESPSensePlug(sid, mac, name, conf_voltage);
    addPlug(plug);
  }
  
  ESPSense(Sensor *sid, std::string mac, std::string alias, float voltage = 120.0) : Component() {
    // Generate single plug
    ESPSensePlug plug = ESPSensePlug(sid, mac, alias, voltage);
    addPlug(plug);
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
  
  void addPlug(ESPSensePlug plug) {
    if(plug_count > (MAX_PLUG_COUNT - 1)) {
      ESP_LOGW("ESPSense", "Attempted to add more than %ui plugs, ignoring", plug_count);
    }
    plugs.push_back(plug);
    plug_count++;
  }
  
private:
  float voltage;
  char response_buf[RES_SIZE];
  std::vector<ESPSensePlug> plugs;
  uint plug_count = 0;
  
  StaticJsonBuffer<200> jsonBuffer;
  
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
      for(auto plug = begin(plugs); plug != end(plugs); ++plug) {
        // Generate JSON response string
        int response_len = plug->generate_response(response_buf);
        // Encrypt
        char encrypted[response_len];
        encrypt(response_buf, response_len, encrypted);
        // Respond to request
        packet.write((uint8_t *)encrypted, response_len);
      }
    }
  }
  
  void decrypt(const uint8_t *data, size_t len, char* result) {
    uint8_t key = 171;
    uint8_t a;
    for (int i = 0; i < len; i++) {
      uint8_t unt = data[i];
      a = unt ^ key;
      key = unt;
      result[i] = char(a);
    }
  }
  
  void encrypt(const char *data, size_t len, char* result) {
    uint8_t key = 171;
    uint8_t a;
    for (int i = 0; i < len; i++) {
      uint8_t unt = data[i];
      a = unt ^ key;
      key = a;
      result[i] = a;
    }
  }
};