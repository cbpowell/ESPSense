#include "esphome.h"
#include "ESPAsyncUDP.h"
#include "ArduinoJson.h"

#define RES_SIZE 400

class ESPSense : public Component {
public:
  AsyncUDP udp;
  Sensor* sensor_id = NULL;
  Switch* switch_id = NULL;
  
  float watts_on = 0;
  float watts_off = 0;
  
  
  ESPSense(Sensor *sid, float voltage) : Component() {
    voltage = voltage;
    sensor_id = sid;
  }
  
  ESPSense(Switch *sid, float voltage, float watts_on, float watts_off) : Component() {
    switch_id = sid;
    voltage = voltage;
    watts_on = watts_on;
    watts_off = watts_off;
  }

  void setup() override {
    if(udp.listen(9999)) {
      ESP_LOGI("ESPSense","Listening on port 9999");
      // Parse incoming packets
      start_sense_response();
    } else {
      ESP_LOGD("ESPSense", "Failed to start UDP listen!");
    }
  }
  
private:
  float voltage;
  char response_buf[RES_SIZE];
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
    char result[packet.length()];
    
    // Decrypt
    decrypt(packet.data(), packet.length(), result);
    ESP_LOGD("ESPSense", "Got message: %s", result);
    
    // Parse JSON
    json::parse_json(result,[&](JsonObject &req) {
      JsonVariant request = req["emeter"]["get_realtime"];
      if (request.success()) {
        ESP_LOGD("ESPSense", "Power measurement requested");
        // Generate JSON response string
        float power = get_power();
        int response_len = generate_response(response_buf, power);
        // Encrypt
        char encrypted[response_len];
        encrypt(response_buf, response_len, encrypted);
        // Respond to request
        packet.write((uint8_t *)encrypted, response_len);
      }
    });
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
    if(sensor_id && id(sensor_id).has_state()) {
      float state = state = id(sensor_id).state;
      return state;
    } else if (switch_id) {
      if(id(switch_id).state) {
        return watts_on;
      } else {
        return watts_off;
      }
    } else {
      return 0.0;
    }
  }
  
  int generate_response(char* data, float power) {
    const char* name = App.get_name().c_str();
    const char* mac = get_mac_address_pretty().c_str();
    
    float current = power / voltage;
    int response_len = snprintf(data, RES_SIZE, base_json.c_str(), current, voltage, power, mac, mac, name);
    ESP_LOGD("ESPSense", "JSON out: %s", data);
    return response_len;
  }
};