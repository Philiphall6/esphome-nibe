#include "NibeGwEcs.h"

#include <algorithm>
#include <cmath>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace nibegw {

static const char *const TAG = "nibegw.ecs";

static request_data_type set_u16(uint16_t value) {
  return {(uint8_t) (value & 0xff), (uint8_t) ((value >> 8) & 0xff)};
}

static request_data_type build_request_data(uint8_t token, const request_data_type &payload) {
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

uint16_t NibeGwEcs::sensor_raw_(sensor::Sensor *sensor) const {
  if (sensor == nullptr || !sensor->has_state() || std::isnan(sensor->state)) {
    return RAW_INVALID;
  }

  auto raw = (int) lroundf(sensor->state);
  raw = std::clamp(raw, 0, (int) RAW_INVALID);
  return (uint16_t) raw;
}

request_data_type NibeGwEcs::build_response_() const {
  request_data_type payload;
  auto append = [&payload](uint16_t value) {
    auto bytes = set_u16(value);
    payload.insert(payload.end(), bytes.begin(), bytes.end());
  };

  append(sensor_raw_(this->bt2_raw_sensor_));
  append(sensor_raw_(this->bt3_raw_sensor_));
  append(sensor_raw_(this->bt50_raw_sensor_));
  append(RAW_INVALID);
  append(RAW_INVALID);
  append(RAW_INVALID);
  append(RAW_INVALID);
  append(RAW_INVALID);

  return build_request_data(ECS_DATA_REQ, payload);
}

void NibeGwEcs::setup() {
  if (this->gw_ == nullptr) {
    ESP_LOGE(TAG, "No NibeGw gateway configured");
    return;
  }

  this->gw_->set_request(this->address_, ECS_DATA_REQ, [this] {
    auto response = this->build_response_();
    ESP_LOGD(TAG, "ECS 0x90 addr=0x%x BT2=%u BT3=%u BT50=%u",
             this->address_, this->sensor_raw_(this->bt2_raw_sensor_), this->sensor_raw_(this->bt3_raw_sensor_),
             this->sensor_raw_(this->bt50_raw_sensor_));
    return response;
  });
  this->gw_->add_acknowledge(this->address_);
}

void NibeGwEcs::dump_config() {
  ESP_LOGCONFIG(TAG, "NibeGw ECS");
  ESP_LOGCONFIG(TAG, "  Address: 0x%x", this->address_);
  LOG_SENSOR("  ", "BT2 raw", this->bt2_raw_sensor_);
  LOG_SENSOR("  ", "BT3 raw", this->bt3_raw_sensor_);
  LOG_SENSOR("  ", "BT50 raw", this->bt50_raw_sensor_);
}

}  // namespace nibegw
}  // namespace esphome
