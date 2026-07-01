#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
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
  void set_bt2_raw_default(uint16_t value) { this->bt2_raw_default_ = clamp_raw_(value); }
  void set_bt3_raw_default(uint16_t value) { this->bt3_raw_default_ = clamp_raw_(value); }
  void set_bt50_raw_default(uint16_t value) { this->bt50_raw_default_ = clamp_raw_(value); }

  void setup() override {
    if (this->gw_ == nullptr) {
      ESP_LOGE(TAG, "No NibeGw gateway configured");
      return;
    }

    this->gw_->set_request(this->address_, ECS_DATA_REQ, [this] {
      auto response = this->build_response_();
      ESP_LOGD(TAG, "ECS 0x90 addr=0x%x BT2=%u BT3=%u BT50=%u", this->address_,
               this->sensor_raw_(this->bt2_raw_sensor_, this->bt2_raw_default_),
               this->sensor_raw_(this->bt3_raw_sensor_, this->bt3_raw_default_),
               this->sensor_raw_(this->bt50_raw_sensor_, this->bt50_raw_default_));
      return response;
    });
    this->gw_->add_acknowledge(this->address_);
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "NibeGw ECS");
    ESP_LOGCONFIG(TAG, "  Address: 0x%x", this->address_);
    LOG_SENSOR("  ", "BT2 raw", this->bt2_raw_sensor_);
    LOG_SENSOR("  ", "BT3 raw", this->bt3_raw_sensor_);
    LOG_SENSOR("  ", "BT50 raw", this->bt50_raw_sensor_);
    ESP_LOGCONFIG(TAG, "  BT2 raw default: %u", this->bt2_raw_default_);
    ESP_LOGCONFIG(TAG, "  BT3 raw default: %u", this->bt3_raw_default_);
    ESP_LOGCONFIG(TAG, "  BT50 raw default: %u", this->bt50_raw_default_);
  }

 protected:
  static constexpr const char *TAG = "nibegw.ecs";
  static constexpr uint16_t RAW_INVALID = 0x03FF;

  NibeGwComponent *gw_{nullptr};
  uint16_t address_{ECS_S3};
  sensor::Sensor *bt2_raw_sensor_{nullptr};
  sensor::Sensor *bt3_raw_sensor_{nullptr};
  sensor::Sensor *bt50_raw_sensor_{nullptr};
  uint16_t bt2_raw_default_{704};
  uint16_t bt3_raw_default_{724};
  uint16_t bt50_raw_default_{RAW_INVALID};

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

  uint16_t sensor_raw_(sensor::Sensor *sensor, uint16_t default_value) const {
    if (sensor == nullptr || !sensor->has_state() || std::isnan(sensor->state)) {
      return default_value;
    }

    auto raw = (int) lroundf(sensor->state);
    raw = std::clamp(raw, 0, (int) RAW_INVALID);
    return (uint16_t) raw;
  }

  request_data_type build_response_() const {
    request_data_type payload;
    auto append = [&payload](uint16_t value) {
      auto bytes = set_u16_(value);
      payload.insert(payload.end(), bytes.begin(), bytes.end());
    };

    append(sensor_raw_(this->bt2_raw_sensor_, this->bt2_raw_default_));
    append(sensor_raw_(this->bt3_raw_sensor_, this->bt3_raw_default_));
    append(sensor_raw_(this->bt50_raw_sensor_, this->bt50_raw_default_));
    append(RAW_INVALID);
    append(RAW_INVALID);
    append(RAW_INVALID);
    append(RAW_INVALID);
    append(RAW_INVALID);

    return build_request_data_(ECS_DATA_REQ, payload);
  }
};

}  // namespace nibegw
}  // namespace esphome
