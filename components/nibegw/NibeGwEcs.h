#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"

#include "NibeGw.h"
#include "NibeGwComponent.h"

namespace esphome {
namespace nibegw {

class NibeGwEcs : public esphome::Component {
 public:
  void set_gw(NibeGwComponent *gw) { this->gw_ = gw; }
  void set_address(uint16_t address) { this->address_ = address; }
  void set_bt2_raw_sensor(sensor::Sensor *sensor) { this->bt2_raw_sensor_ = sensor; }
  void set_bt3_raw_sensor(sensor::Sensor *sensor) { this->bt3_raw_sensor_ = sensor; }
  void set_bt50_raw_sensor(sensor::Sensor *sensor) { this->bt50_raw_sensor_ = sensor; }
  void set_w3_raw_sensor(sensor::Sensor *sensor) { this->w3_raw_sensor_ = sensor; }
  void set_w4_raw_sensor(sensor::Sensor *sensor) { this->w4_raw_sensor_ = sensor; }
  void set_w5_raw_sensor(sensor::Sensor *sensor) { this->w5_raw_sensor_ = sensor; }
  void set_w6_raw_sensor(sensor::Sensor *sensor) { this->w6_raw_sensor_ = sensor; }
  void set_w7_raw_sensor(sensor::Sensor *sensor) { this->w7_raw_sensor_ = sensor; }
  void set_bt2_raw_default(uint16_t value) { this->bt2_raw_default_ = clamp_raw_(value); }
  void set_bt3_raw_default(uint16_t value) { this->bt3_raw_default_ = clamp_raw_(value); }
  void set_bt50_raw_default(uint16_t value) { this->bt50_raw_default_ = clamp_raw_(value); }
  void set_w3_raw_default(uint16_t value) { this->w3_raw_default_ = clamp_raw_(value); }
  void set_w4_raw_default(uint16_t value) { this->w4_raw_default_ = clamp_raw_(value); }
  void set_w5_raw_default(uint16_t value) { this->w5_raw_default_ = clamp_raw_(value); }
  void set_w6_raw_default(uint16_t value) { this->w6_raw_default_ = clamp_raw_(value); }
  void set_w7_raw_default(uint16_t value) { this->w7_raw_default_ = clamp_raw_(value); }
  void set_msg1_binary_sensor(binary_sensor::BinarySensor *sensor) { this->msg1_binary_sensor_ = sensor; }
  void set_msg1_binary_byte(uint8_t value) { this->msg1_binary_byte_ = value; }
  void set_msg1_binary_mask(uint8_t value) { this->msg1_binary_mask_ = value; }

  void setup() override {
    if (this->gw_ == nullptr) {
      ESP_LOGE(TAG, "No NibeGw gateway configured");
      return;
    }

    this->bt2_raw_cached_ = this->bt2_raw_default_;
    this->bt3_raw_cached_ = this->bt3_raw_default_;
    this->bt50_raw_cached_ = this->bt50_raw_default_;
    this->w3_raw_cached_ = this->w3_raw_default_;
    this->w4_raw_cached_ = this->w4_raw_default_;
    this->w5_raw_cached_ = this->w5_raw_default_;
    this->w6_raw_cached_ = this->w6_raw_default_;
    this->w7_raw_cached_ = this->w7_raw_default_;

    this->attach_sensor_cache_(this->bt2_raw_sensor_, this->bt2_raw_cached_, "BT2");
    this->attach_sensor_cache_(this->bt3_raw_sensor_, this->bt3_raw_cached_, "BT3");
    this->attach_sensor_cache_(this->bt50_raw_sensor_, this->bt50_raw_cached_, "BT50");
    this->attach_sensor_cache_(this->w3_raw_sensor_, this->w3_raw_cached_, "W3");
    this->attach_sensor_cache_(this->w4_raw_sensor_, this->w4_raw_cached_, "W4");
    this->attach_sensor_cache_(this->w5_raw_sensor_, this->w5_raw_cached_, "W5");
    this->attach_sensor_cache_(this->w6_raw_sensor_, this->w6_raw_cached_, "W6");
    this->attach_sensor_cache_(this->w7_raw_sensor_, this->w7_raw_cached_, "W7");
    this->refresh_cache_from_sensors_();

    this->gw_->set_request(this->address_, ECS_DATA_REQ, [this] {
      this->refresh_cache_from_sensors_();
      auto response = this->build_response_();
      ESP_LOGD(TAG, "ECS 0x90 addr=0x%x W0=%u W1=%u W2=%u W3=%u W4=%u W5=%u W6=%u W7=%u", this->address_,
               this->bt2_raw_cached_, this->bt3_raw_cached_, this->bt50_raw_cached_, this->w3_raw_cached_,
               this->w4_raw_cached_, this->w5_raw_cached_, this->w6_raw_cached_, this->w7_raw_cached_);
      return response;
    });
    this->gw_->add_acknowledge(this->address_);

    if (this->msg1_binary_sensor_ != nullptr && this->msg1_binary_mask_ != 0) {
      this->gw_->add_listener(this->address_, ECS_DATA_MSG_1, [this](const request_data_type &message) {
        if (message.size() <= this->msg1_binary_byte_) {
          ESP_LOGW(TAG, "ECS 0x55 addr=0x%x invalid length=%zu", this->address_, message.size());
          return;
        }
        bool state = (message[this->msg1_binary_byte_] & this->msg1_binary_mask_) != 0;
        ESP_LOGD(TAG, "ECS 0x55 addr=0x%x byte=%u mask=0x%02x state=%s", this->address_, this->msg1_binary_byte_,
                 this->msg1_binary_mask_, ONOFF(state));
        this->msg1_binary_sensor_->publish_state(state);
      });
    }
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "NibeGw ECS");
    ESP_LOGCONFIG(TAG, "  Address: 0x%x", this->address_);
    LOG_SENSOR("  ", "BT2 raw", this->bt2_raw_sensor_);
    LOG_SENSOR("  ", "BT3 raw", this->bt3_raw_sensor_);
    LOG_SENSOR("  ", "BT50 raw", this->bt50_raw_sensor_);
    LOG_SENSOR("  ", "W3 raw", this->w3_raw_sensor_);
    LOG_SENSOR("  ", "W4 raw", this->w4_raw_sensor_);
    LOG_SENSOR("  ", "W5 raw", this->w5_raw_sensor_);
    LOG_SENSOR("  ", "W6 raw", this->w6_raw_sensor_);
    LOG_SENSOR("  ", "W7 raw", this->w7_raw_sensor_);
    ESP_LOGCONFIG(TAG, "  BT2 raw default: %u", this->bt2_raw_default_);
    ESP_LOGCONFIG(TAG, "  BT3 raw default: %u", this->bt3_raw_default_);
    ESP_LOGCONFIG(TAG, "  BT50 raw default: %u", this->bt50_raw_default_);
    ESP_LOGCONFIG(TAG, "  W3 raw default: %u", this->w3_raw_default_);
    ESP_LOGCONFIG(TAG, "  W4 raw default: %u", this->w4_raw_default_);
    ESP_LOGCONFIG(TAG, "  W5 raw default: %u", this->w5_raw_default_);
    ESP_LOGCONFIG(TAG, "  W6 raw default: %u", this->w6_raw_default_);
    ESP_LOGCONFIG(TAG, "  W7 raw default: %u", this->w7_raw_default_);
    LOG_BINARY_SENSOR("  ", "0x55 binary", this->msg1_binary_sensor_);
    ESP_LOGCONFIG(TAG, "  0x55 binary byte: %u", this->msg1_binary_byte_);
    ESP_LOGCONFIG(TAG, "  0x55 binary mask: 0x%02x", this->msg1_binary_mask_);
  }

 protected:
  static constexpr const char *TAG = "nibegw.ecs";
  static constexpr uint16_t RAW_INVALID = 0x03FF;

  NibeGwComponent *gw_{nullptr};
  uint16_t address_{ECS_S3};
  sensor::Sensor *bt2_raw_sensor_{nullptr};
  sensor::Sensor *bt3_raw_sensor_{nullptr};
  sensor::Sensor *bt50_raw_sensor_{nullptr};
  sensor::Sensor *w3_raw_sensor_{nullptr};
  sensor::Sensor *w4_raw_sensor_{nullptr};
  sensor::Sensor *w5_raw_sensor_{nullptr};
  sensor::Sensor *w6_raw_sensor_{nullptr};
  sensor::Sensor *w7_raw_sensor_{nullptr};
  uint16_t bt2_raw_default_{704};
  uint16_t bt3_raw_default_{724};
  uint16_t bt50_raw_default_{RAW_INVALID};
  uint16_t w3_raw_default_{RAW_INVALID};
  uint16_t w4_raw_default_{RAW_INVALID};
  uint16_t w5_raw_default_{RAW_INVALID};
  uint16_t w6_raw_default_{RAW_INVALID};
  uint16_t w7_raw_default_{RAW_INVALID};
  uint16_t bt2_raw_cached_{704};
  uint16_t bt3_raw_cached_{724};
  uint16_t bt50_raw_cached_{RAW_INVALID};
  uint16_t w3_raw_cached_{RAW_INVALID};
  uint16_t w4_raw_cached_{RAW_INVALID};
  uint16_t w5_raw_cached_{RAW_INVALID};
  uint16_t w6_raw_cached_{RAW_INVALID};
  uint16_t w7_raw_cached_{RAW_INVALID};
  binary_sensor::BinarySensor *msg1_binary_sensor_{nullptr};
  uint8_t msg1_binary_byte_{0};
  uint8_t msg1_binary_mask_{0};

  static uint16_t clamp_raw_(uint16_t value) {
    return std::min(value, RAW_INVALID);
  }

  static request_data_type set_u16_(uint16_t value) {
    return {(uint8_t) (value & 0xff), (uint8_t) ((value >> 8) & 0xff)};
  }

  static request_data_type build_request_data_(uint8_t token, const request_data_type &payload) {
    request_data_type data = {
        STARTBYTE_SLAVE,
        token,
        (uint8_t) payload.size(),
    };

    data.insert(data.end(), payload.begin(), payload.end());

    uint8_t checksum = 0;
    for (auto val : data)
      checksum ^= val;
    if (checksum == STARTBYTE_MASTER)
      checksum = 0xC5;
    data.push_back(checksum);
    return data;
  }

  bool sensor_raw_(sensor::Sensor *sensor, uint16_t &raw) const {
    if (sensor == nullptr || !sensor->has_state() || std::isnan(sensor->state)) {
      return false;
    }

    auto value = (int) lroundf(sensor->state);
    value = std::clamp(value, 0, (int) RAW_INVALID);
    raw = (uint16_t) value;
    return true;
  }

  void attach_sensor_cache_(sensor::Sensor *sensor, uint16_t &cached, const char *name) {
    if (sensor == nullptr) {
      return;
    }

    auto *cached_ptr = &cached;
    sensor->add_on_state_callback([this, cached_ptr, name](float state) {
      if (std::isnan(state)) {
        ESP_LOGD(TAG, "Ignoring NaN ECS %s raw value, keeping %u", name, *cached_ptr);
        return;
      }

      auto raw = (int) lroundf(state);
      raw = std::clamp(raw, 0, (int) RAW_INVALID);
      *cached_ptr = (uint16_t) raw;
      ESP_LOGD(TAG, "Cached ECS %s raw=%u", name, *cached_ptr);
    });
  }

  void refresh_cache_from_sensors_() {
    uint16_t raw;
    if (this->sensor_raw_(this->bt2_raw_sensor_, raw))
      this->bt2_raw_cached_ = raw;
    if (this->sensor_raw_(this->bt3_raw_sensor_, raw))
      this->bt3_raw_cached_ = raw;
    if (this->sensor_raw_(this->bt50_raw_sensor_, raw))
      this->bt50_raw_cached_ = raw;
    if (this->sensor_raw_(this->w3_raw_sensor_, raw))
      this->w3_raw_cached_ = raw;
    if (this->sensor_raw_(this->w4_raw_sensor_, raw))
      this->w4_raw_cached_ = raw;
    if (this->sensor_raw_(this->w5_raw_sensor_, raw))
      this->w5_raw_cached_ = raw;
    if (this->sensor_raw_(this->w6_raw_sensor_, raw))
      this->w6_raw_cached_ = raw;
    if (this->sensor_raw_(this->w7_raw_sensor_, raw))
      this->w7_raw_cached_ = raw;
  }

  request_data_type build_response_() {
    request_data_type payload;
    auto append = [&payload](uint16_t value) {
      auto bytes = set_u16_(value);
      payload.insert(payload.end(), bytes.begin(), bytes.end());
    };

    append(this->bt2_raw_cached_);
    append(this->bt3_raw_cached_);
    append(this->bt50_raw_cached_);
    append(this->w3_raw_cached_);
    append(this->w4_raw_cached_);
    append(this->w5_raw_cached_);
    append(this->w6_raw_cached_);
    append(this->w7_raw_cached_);

    return build_request_data_(ECS_DATA_REQ, payload);
  }
};

}  // namespace nibegw
}  // namespace esphome
