// Copyright 2022, Charles Powell

#include "esphome/components/json/json_util.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/socket/socket.h"
#include "esphome/components/socket/headers.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include <memory>
#include <cstring>
#include <cstdio>

namespace esphome {
namespace espsense {

#define RES_SIZE 400
#define REQ_SIZE 70
#define MAX_PLUG_COUNT 10  // Somewhat arbitrary as of now

class ESPSensePlug {
 public:
  std::string name;
  std::string mac;
  bool encrypt = true;
  float voltage = 120.0;
  sensor::Sensor *power_sid = NULL;
  sensor::Sensor *voltage_sid = NULL;
  sensor::Sensor *current_sid = NULL;
  
  std::string base_json = "{\"emeter\": {\"get_realtime\":{ "
                              "\"current\": %.02f, \"voltage\": %.02f, \"power\": %.02f, \"total\": 0, \"err_code\": 0}}, "
                           "\"system\": {\"get_sysinfo\": "
                              "{\"err_code\": 0, \"hw_ver\": 1.0, \"type\": \"IOT.SMARTPLUGSWITCH\", \"model\": \"HS110(US)\", "
                           "\"mac\": \"%s\", \"deviceId\": \"%s\", \"alias\": \"%s\", \"relay_state\": 1, \"updating\": 0 }}}";

  ESPSensePlug() {}

  void set_name(std::string name) { this->name = name; }
  void set_mac_address(std::string mac) { this->mac = mac; }
  void set_encrypt(bool encrypt) { this->encrypt = encrypt; }
  void set_voltage(float voltage) { this->voltage = voltage; }
  void set_power_sensor(sensor::Sensor *sensor) { this->power_sid = sensor; }
  void set_voltage_sensor(sensor::Sensor *sensor) { this->voltage_sid = sensor; }
  void set_current_sensor(sensor::Sensor *sensor) { this->current_sid = sensor; }

  float get_power() {
    return get_sensor_reading(power_sid, 0.0);
  }
  
  float get_voltage() {
    return get_sensor_reading(voltage_sid, voltage);
  }
  
  float get_current() {
    return get_sensor_reading(current_sid, get_power() / get_voltage());
  }
  
  float get_sensor_reading(sensor::Sensor *sid, float default_value) {
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
  ESPSense() : Component() {}
  
  float get_setup_priority() const override { return esphome::setup_priority::AFTER_WIFI; }
  
  void setup() override {
    this->socket_ = socket::socket_ip(SOCK_DGRAM, IPPROTO_IP);
    if (this->socket_ == nullptr) {
      ESP_LOGE("ESPSense", "Could not create socket");
      this->mark_failed();
      return;
    }

    int enable = 1;
    int err = this->socket_->setsockopt(SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int));
    if (err != 0) {
      ESP_LOGW("ESPSense", "Socket unable to set reuseaddr: errno %d", err);
      // we can still continue
    }
    
    err = this->socket_->setblocking(false);
    if (err != 0) {
      ESP_LOGW("ESPSense", "Socket unable to set nonblocking mode: errno %d", err);
      this->mark_failed();
      return;
    }

    struct sockaddr_storage server;
    socklen_t sl = socket::set_sockaddr_any((struct sockaddr *) &server, sizeof(server), 9999);
    if (sl == 0) {
      ESP_LOGE("ESPSense", "Socket unable to set sockaddr: errno %d", errno);
      this->mark_failed();
      return;
    }

    err = this->socket_->bind((struct sockaddr *) &server, sizeof(server));
    if (err != 0) {
      ESP_LOGE("ESPSense", "Socket unable to bind: errno %d", errno);
      this->mark_failed();
      return;
    }

    ESP_LOGI("ESPSense","Listening on port 9999");
  }

  void loop() override {
    if (this->socket_ == nullptr) {
      return;
    }

    uint8_t buf[REQ_SIZE];
    struct sockaddr_storage remote_addr;
    socklen_t remote_addr_len = sizeof(remote_addr);

#ifdef USE_SOCKET_IMPL_BSD_SOCKETS
    ssize_t len = this->socket_->recvfrom(buf, sizeof(buf), (struct sockaddr *)&remote_addr, &remote_addr_len);
#else
    // Fallback to read() if recvfrom is not available
    ssize_t len = this->socket_->read(buf, sizeof(buf));
    remote_addr_len = 0;
#endif

    if (len == -1) {
      // No packet available or error (non-blocking socket)
      return;
    }

    if (len > REQ_SIZE) {
      ESP_LOGD("ESPSense", "Packet is oversized, ignoring");
      return;
    }

    // Get remote IP address for logging (compatible with both Arduino and ESP-IDF)
    char remote_ip_str[46] = "unknown";
    if (remote_addr_len > 0 && remote_addr.ss_family == AF_INET) {
      struct sockaddr_in *addr_in = (struct sockaddr_in *)&remote_addr;
      // Access IP address bytes directly - works with both lwIP and BSD sockets
      uint8_t *ip_bytes = (uint8_t *)&addr_in->sin_addr;
      #ifdef USE_SOCKET_IMPL_LWIP_TCP
        // lwIP structure - access bytes directly
        snprintf(remote_ip_str, sizeof(remote_ip_str), "%d.%d.%d.%d",
                 ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]);
      #else
        // BSD sockets - sin_addr.s_addr is uint32_t in network byte order
        uint32_t ip_addr;
        #ifdef USE_SOCKET_IMPL_BSD_SOCKETS
          ip_addr = addr_in->sin_addr.s_addr;
          // ntohl should be available via socket headers
          ip_addr = ntohl(ip_addr);
        #else
          // Fallback: access as bytes
          memcpy(&ip_addr, &addr_in->sin_addr.s_addr, sizeof(uint32_t));
          // Manual byte swap for network to host
          ip_addr = ((ip_addr & 0xFF000000) >> 24) | ((ip_addr & 0x00FF0000) >> 8) |
                    ((ip_addr & 0x0000FF00) << 8) | ((ip_addr & 0x000000FF) << 24);
        #endif
        snprintf(remote_ip_str, sizeof(remote_ip_str), "%d.%d.%d.%d",
                 (ip_addr >> 24) & 0xFF, (ip_addr >> 16) & 0xFF, 
                 (ip_addr >> 8) & 0xFF, ip_addr & 0xFF);
      #endif
    }

    ESP_LOGD("ESPSense", "Got packet from %s", remote_ip_str);
    parse_packet(buf, len, &remote_addr, remote_addr_len);
  }

  void addPlug(ESPSensePlug *plug) {
    if (plugs.size() >= MAX_PLUG_COUNT) {
      ESP_LOGW("ESPSense", "Attempted to add more than %ui plugs, ignoring", MAX_PLUG_COUNT);
    }

    if (plug->mac.empty())
    {
      if (plugs.size() == 0)
      {
        // First plug to be added, and no MAC set, so default to own hardware MAC
        plug->set_mac_address(get_mac_address_pretty());
      } else {
        // Generate a fake MAC address from the name to prevent issues when there are multiple plugs with the same MAC address
        uint32_t name_hash = fnv1_hash(plug->name);
        uint8_t *hash_pointer = (uint8_t *)&name_hash;
        char mac[20];
        sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", 53, 75, hash_pointer[0], hash_pointer[1], hash_pointer[2], hash_pointer[3]);
        plug->set_mac_address(mac);
      }
    }

    plugs.push_back(plug);
  }
  
private:
  float voltage;
  char response_buf[RES_SIZE];
  std::vector<ESPSensePlug *> plugs;
  std::unique_ptr<socket::Socket> socket_;

#if ESPHOME_VERSION_CODE < VERSION_CODE(2022, 1, 0) 
  StaticJsonBuffer<200> jsonBuffer;
#endif
  
  void parse_packet(const uint8_t *data, size_t len, struct sockaddr_storage *remote_addr, socklen_t remote_addr_len) {
    if(len > REQ_SIZE) {
      // Not a Sense request packet
      ESP_LOGD("ESPSense", "Packet is oversized, ignoring");
      return;
    }
    
    char request_buf[REQ_SIZE];
    
    // Decrypt
    decrypt(data, len, request_buf);
    
    // Add null terminator
    request_buf[len] = '\0';
    
    // Print into null-terminated string if verbose debugging
    ESP_LOGV("ESPSense", "Message: %s", request_buf);
    
    // Parse JSON
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2022, 1, 0)
    // ArduinoJson 6
    StaticJsonDocument<200> jsonDoc, emeterDoc;
    auto jsonError = deserializeJson(jsonDoc, request_buf);
    if(jsonError) {
      ESP_LOGW("ESPSense", "JSON parse failed! Error: %s", jsonError.c_str());
      return;
    }
    ESP_LOGD("ESPSense", "Parse of message JSON successful");
    
    // Check if this is a valid request by looking for emeter key
    if (!jsonDoc["emeter"]["get_realtime"]) {
      ESP_LOGD("ESPSense", "Message was not deserialized as a request for power measurement");
    } else {
#else
    // ArduinoJson 5
    jsonBuffer.clear();
    JsonObject &req = jsonBuffer.parseObject(request_buf);
    if(!req.success()) {
      ESP_LOGW("ESPSense", "JSON parse failed!");
      return;
    }
    ESP_LOGD("ESPSense", "Parse of message JSON successful");
    
    // Check if this is a valid request by looking for emeter key
    JsonVariant request = req["emeter"]["get_realtime"];
    if (!request.success()) {
      ESP_LOGD("ESPSense", "Message not a request for power measurement");
    } else {
#endif
      ESP_LOGD("ESPSense", "Power measurement requested");
      for (auto *plug : this->plugs) {
        // Generate JSON response string
        int response_len = plug->generate_response(response_buf);
        char response[response_len];
        if (plug->encrypt) {
          // Encrypt
          encrypt(response_buf, response_len, response);
          // Respond to request
          this->socket_->sendto((uint8_t *)response, response_len, 0, 
                                (struct sockaddr *)remote_addr, remote_addr_len);
        } else {
          // Response to request
          this->socket_->sendto((uint8_t *)response_buf, response_len, 0,
                                (struct sockaddr *)remote_addr, remote_addr_len);
        }
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

}  // namespace espsense
}  // namespace esphome
