"""Compile the production ECS header against minimal ESPHome/gateway test doubles."""
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class EcsRuntimeTest(unittest.TestCase):
    def test_passive_active_and_frames(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            def put(path, data):
                target = root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(data)
            put('NibeGwEcs.h', (ROOT / 'components/nibegw/NibeGwEcs.h').read_text())
            put('esphome/core/component.h', '#pragma once\nnamespace esphome { class Component { public: virtual void setup() {} virtual void dump_config() {} }; }')
            put('esphome/core/log.h', '#pragma once\n#define ESP_LOGE(...)\n#define ESP_LOGW(...)\n#define ESP_LOGD(...)\n#define ESP_LOGCONFIG(...)\n#define LOG_SENSOR(...)\n#define LOG_BINARY_SENSOR(...)\n#define ONOFF(x) ((x)?"ON":"OFF")\n')
            put('esphome/components/sensor/sensor.h', '''#pragma once
#include <functional>
namespace esphome::sensor { class Sensor { public: float state=0; bool has_state() { return false; } void add_on_state_callback(std::function<void(float)>) {} }; }
''')
            put('esphome/components/binary_sensor/binary_sensor.h', '''#pragma once
namespace esphome::binary_sensor { class BinarySensor { public: bool state=false; int publishes=0; void publish_state(bool value) { state=value; ++publishes; } }; }
''')
            put('esphome/components/text_sensor/text_sensor.h', '''#pragma once
#include <string>
namespace esphome::text_sensor { class TextSensor { public: std::string state; void publish_state(std::string value) { state=value; } }; }
''')
            put('NibeGw.h', '''#pragma once
#include <vector>
#include <cstdint>
namespace esphome::nibegw {
using request_data_type = std::vector<uint8_t>;
constexpr uint8_t ECS_S3=3, ECS_DATA_REQ=0x90, ECS_DATA_MSG_1=0x55, ECS_DATA_MSG_2=0xa0, STARTBYTE_SLAVE=0xc0, STARTBYTE_MASTER=0x5c;
}
''')
            put('NibeGwComponent.h', '''#pragma once
#include <functional>
#include <map>
#include "NibeGw.h"
namespace esphome::nibegw { class NibeGwComponent { public:
int requests=0, acks=0;
std::map<std::pair<int,int>,std::function<void(const request_data_type &)>> listeners;
std::function<request_data_type()> reply;
void set_request(int, int, std::function<request_data_type()> cb) {++requests; reply=cb;}
void add_acknowledge(int) {++acks;}
void add_listener(int a,int t,std::function<void(const request_data_type &)> cb) {listeners[{a,t}]=cb;}
}; }
''')
            put('test.cpp', '''#include <cassert>
#include "NibeGwEcs.h"
using namespace esphome;
using namespace esphome::nibegw;
int main() {
NibeGwComponent gw;
NibeGwEcs s2;
text_sensor::TextSensor raw, raw2;
binary_sensor::BinarySensor pump, opening, closing;
s2.set_gw(&gw); s2.set_address(2); s2.set_observe_only(true);
s2.set_msg1_raw_sensor(&raw); s2.set_msg2_raw_sensor(&raw2);
s2.set_msg1_pump_request_sensor(&pump);
s2.set_msg1_mixing_open_sensor(&opening); s2.set_msg1_mixing_close_sensor(&closing);
s2.setup(); assert(gw.acks==0 && gw.requests==0);
auto &rx=gw.listeners.at({2,0x55});
rx({1,5}); assert(pump.state && opening.state && !closing.state);
rx({0,6}); assert(!pump.state && !opening.state && closing.state);
rx({2,0x77}); assert(raw.state=="02 77" && !opening.state && !closing.state);
int count=pump.publishes;
rx({}); rx({1}); assert(pump.publishes==count);
rx({2,0x5c,0xab}); assert(raw.state=="02 5C AB");
gw.listeners.at({2,0xa0})({0,0,0xff}); assert(raw2.state=="00 00 FF");
NibeGwEcs s3; s3.set_gw(&gw); s3.set_address(3); s3.setup();
assert(gw.acks==1 && gw.requests==1);
auto reply=gw.reply(); assert(reply.size()==20 && reply[1]==0x90 && reply[2]==16);
assert(reply[3]==0xc0 && reply[4]==2); // default W0=704
// S3 registration leaves the passive S2 listener intact.
rx({1,0}); assert(pump.state && gw.acks==1);
}
''')
            subprocess.run(['g++', '-std=c++17', '-I', str(root), str(root/'test.cpp'), '-o', str(root/'test')], check=True)
            subprocess.run([str(root/'test')], check=True)

if __name__ == '__main__':
    unittest.main()
