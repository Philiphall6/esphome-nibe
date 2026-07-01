#pragma once

#include <cstdint>

#include "esphome/core/component.h"
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

  void setup() override;
  void dump_config() override;

 protected:
  static constexpr uint16_t RAW_INVALID = 0x03FF;

  NibeGwComponent *gw_{nullptr};
  uint16_t address_{ECS_S3};
  sensor::Sensor *bt2_raw_sensor_{nullptr};
  sensor::Sensor *bt3_raw_sensor_{nullptr};
  sensor::Sensor *bt50_raw_sensor_{nullptr};

  uint16_t sensor_raw_(sensor::Sensor *sensor) const;
  request_data_type build_response_() const;
};

}  // namespace nibegw
}  // namespace esphome
