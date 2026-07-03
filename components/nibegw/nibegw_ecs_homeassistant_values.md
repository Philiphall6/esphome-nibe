# NibeGW ECS / AXC style accessories from Home Assistant values

This component can emulate Nibe accessory data blocks that use token `0x90`.

It was tested with:

```text
ECS_S3  = 0x03
POOL310 = 0x06
SCA35   = 0x0A
DEH310  = 0x27
COOLING = 0x2B
```

The `0x90` response contains 8 little-endian 16-bit raw words:

```text
W0 W1 W2 W3 W4 W5 W6 W7
```

Each word is a raw Nibe/NTC value, not a temperature in degC.

```text
704  -> C0 02
724  -> D4 02
1023 -> FF 03  (invalid / sensor not connected)
```

## YAML word mapping

The YAML names map to the `0x90` words like this:

```text
bt2_raw  -> W0
bt3_raw  -> W1
bt50_raw -> W2
w3_raw   -> W3
w4_raw   -> W4
w5_raw   -> W5
w6_raw   -> W6
w7_raw   -> W7
```

The matching defaults are:

```text
bt2_raw_default
bt3_raw_default
bt50_raw_default
w3_raw_default
w4_raw_default
w5_raw_default
w6_raw_default
w7_raw_default
```

Defaults are used at boot and whenever the Home Assistant sensor has not yet provided a valid value.
Once a valid HA value is received, it is cached and reused for later `0x90` responses.

## Temperature raw values

Observed values:

```text
780 = about 16.4 degC
770 = about 16.9 degC
760 = about 18.0 degC
750 = about 19.1 degC
697 = about 25.0 degC
```

Around 19-25 degC, a rough approximation is:

```text
raw ~= 922 - 9 * temperature_c
```

Examples:

```text
24 degC -> raw ~= 706
25 degC -> raw ~= 697
26 degC -> raw ~= 688
```

## ECS_S3 mapping

Confirmed:

```text
W0 = BT2
W1 = BT3
W2 = BT50
```

Example:

```yaml
sensor:
  - platform: homeassistant
    id: s3_bt2_raw
    entity_id: input_number.nibe_s3_bt2_raw
    internal: true

  - platform: homeassistant
    id: s3_bt3_raw
    entity_id: input_number.nibe_s3_bt3_raw
    internal: true

  - platform: homeassistant
    id: s3_bt50_raw
    entity_id: input_number.nibe_s3_bt50_raw
    internal: true

nibegw:
  id: my_nibegw

  acknowledge:
    - MODBUS40
    - RMU40_S1
    - ECS_S3

  ecs:
    - address: ECS_S3
      bt2_raw: s3_bt2_raw
      bt3_raw: s3_bt3_raw
      bt50_raw: s3_bt50_raw
      bt2_raw_default: 704
      bt3_raw_default: 724
      bt50_raw_default: 1023
```

Do not also configure a constant `ECS_DATA_REQ` for the same address. The `ecs:` block provides token `0x90` dynamically.

## POOL310 mapping

Confirmed by tests:

```text
POOL310 W0 / bt2_raw  = BT51 pool return temperature
POOL310 W1 / bt3_raw  = unused / set to 1023 unless proven otherwise
POOL310 W2 / bt50_raw = BT59 pool supply temperature
```

Observed `0x55` data:

```text
00 00 = off
0C 00 = active/heating request + accessory flag
0E 00 = active/heating request + diverter valve + accessory flag
```

For POOL310, `0x08` is not the pump request in the available dumps. It behaves like an accessory flag.
The useful POOL310 byte 0 bits are:

```text
0x02 = diverter/reversing valve request
0x04 = pool active / heating request
0x08 = accessory active / accessory flag
```

Example:

```yaml
sensor:
  - platform: homeassistant
    id: pool_bt51_raw
    entity_id: input_number.nibe_pool_bt51_raw
    internal: false

  - platform: homeassistant
    id: pool_bt59_raw
    entity_id: input_number.nibe_pool_bt59_raw
    internal: false

binary_sensor:
  - platform: template
    id: pool310_heating_request
    name: "POOL310 demande chauffage"

  - platform: template
    id: pool310_valve_request
    name: "POOL310 demande vanne inversion"

  - platform: template
    id: pool310_accessory_active
    name: "POOL310 accessoire actif"

nibegw:
  id: my_nibegw

  acknowledge:
    - POOL310

  ecs:
    - address: POOL310
      bt2_raw: pool_bt51_raw
      bt2_raw_default: 706
      bt3_raw_default: 1023
      bt50_raw: pool_bt59_raw
      bt50_raw_default: 688
      msg1_active_sensor: pool310_heating_request
      msg1_valve_request_sensor: pool310_valve_request
      msg1_accessory_sensor: pool310_accessory_active
```

When the heat pump sends POOL310 states, logs should show:

```text
ECS 0x55 addr=0x6 data=0c 00 pump_request=OFF valve_request=OFF active=ON accessory_active=ON
ECS 0x55 addr=0x6 data=0e 00 pump_request=OFF valve_request=ON active=ON accessory_active=ON
```

## SCA35 and DEH310

These appear to use the same `0x90` format.

Observed idle data:

```text
SCA35  addr=0x0A: W0 around 700, W1 around 699, W2 around 687..703
DEH310 addr=0x27: W0 around 701, W1 around 700, W2 around 700..703
```

SCA35 hot-water/solar test:

```text
SCA35 0x90 W0 changed 750 -> 450 -> 350 -> 150 when the solar panel temperature was raised.
SCA35 0x55 changed 00 00 -> 01 00.
```

So for SCA35:

```text
W0 / bt2_raw  = BT53 solar panel temperature
W1 / bt3_raw  = BT54 solar load temperature
W2 / bt50_raw = EP30-BT51 solar pool temperature / third solar sensor
0x55 byte 0 mask 0x01 = solar pump request
```

Example:

```yaml
sensor:
  - platform: homeassistant
    id: sca_bt53_raw
    entity_id: input_number.nibe_sca_bt53_raw
    internal: false

  - platform: homeassistant
    id: sca_bt54_raw
    entity_id: input_number.nibe_sca_bt54_raw
    internal: false

  - platform: homeassistant
    id: sca_bt51_pool_raw
    entity_id: input_number.nibe_sca_bt51_pool_raw
    internal: false

binary_sensor:
  - platform: template
    id: pool310_heating_request
    name: "POOL310 demande chauffage"

  - platform: template
    id: pool310_valve_request
    name: "POOL310 demande vanne inversion"

  - platform: template
    id: pool310_accessory_active
    name: "POOL310 accessoire actif"

  - platform: template
    id: sca35_pump_request
    name: "SCA35 demande circulateur"

nibegw:
  id: my_nibegw

  acknowledge:
    - SCA35

  ecs:
    - address: SCA35
      bt2_raw: sca_bt53_raw
      bt2_raw_default: 750
      bt3_raw: sca_bt54_raw
      bt3_raw_default: 702
      bt50_raw: sca_bt51_pool_raw
      bt50_raw_default: 687
      msg1_binary_sensor: sca35_pump_request
      msg1_binary_byte: 0
      msg1_binary_mask: 0x01
```

In the available idle dump, DEH310 `0x55` was `00 00`, so its pump bit is still not confirmed.

## 0x55 logging and binary sensors

The component always logs these common byte 0 meanings:

```text
mask 0x01 = pump_request
mask 0x02 = valve_request
mask 0x04 = active
mask 0x08 = accessory_active / accessory flag
```

It can publish the decoded states to Home Assistant as ESPHome `binary_sensor` entities:

```text
msg1_pump_request_sensor     -> byte 0 mask 0x01
msg1_valve_request_sensor    -> byte 0 mask 0x02
msg1_active_sensor           -> byte 0 mask 0x04
msg1_accessory_sensor        -> byte 0 mask 0x08
msg1_mixing_open_sensor      -> configurable value match, default byte 1 == 0x05
msg1_mixing_close_sensor     -> configurable value match, default byte 1 == 0x06
```

The mixing valve mapping is prepared but still needs confirmation from a real opening/closing dump.
If the dump proves different values, change these YAML options:

```yaml
msg1_mixing_open_byte: 1
msg1_mixing_open_mask: 0xFF
msg1_mixing_open_value: 0x05
msg1_mixing_close_byte: 1
msg1_mixing_close_mask: 0xFF
msg1_mixing_close_value: 0x06
```

Example:

```text
ECS 0x55 addr=0xa data=01 00 pump_request=ON valve_request=OFF active=OFF accessory_active=OFF mixing_open=OFF mixing_close=OFF
ECS 0x55 addr=0x2b data=07 00 pump_request=ON valve_request=ON active=ON accessory_active=OFF mixing_open=OFF mixing_close=OFF
ECS 0x55 addr=0x6 data=0c 00 pump_request=OFF valve_request=OFF active=ON accessory_active=ON mixing_open=OFF mixing_close=OFF
ECS 0x55 addr=0x6 data=0e 00 pump_request=OFF valve_request=ON active=ON accessory_active=ON mixing_open=OFF mixing_close=OFF
```

You can publish these decoded states directly:

```yaml
binary_sensor:
  - platform: template
    id: pool310_heating_request
    name: "POOL310 demande chauffage"

  - platform: template
    id: pool310_valve_request
    name: "POOL310 demande vanne inversion"

  - platform: template
    id: pool310_accessory_active
    name: "POOL310 accessoire actif"

  - platform: template
    id: sca35_pump_request
    name: "SCA35 demande circulateur"

  - platform: template
    id: deh310_pump_request
    name: "DEH310 demande circulateur"

  - platform: template
    id: cooling_pump_request
    name: "Cooling demande circulateur"

  - platform: template
    id: cooling_valve_request
    name: "Cooling demande vanne inversion"

  - platform: template
    id: cooling_active
    name: "Cooling actif"

  - platform: template
    id: ecs_s3_accessory_active
    name: "ECS S3 accessoire actif"

  - platform: template
    id: ecs_s3_pump_request
    name: "ECS S3 demande circulateur"

  - platform: template
    id: ecs_s3_mixing_open
    name: "ECS S3 vanne ouverture"

  - platform: template
    id: ecs_s3_mixing_close
    name: "ECS S3 vanne fermeture"

nibegw:
  ecs:
    - address: ECS_S3
      msg1_accessory_sensor: ecs_s3_accessory_active
      msg1_pump_request_sensor: ecs_s3_pump_request
      msg1_mixing_open_sensor: ecs_s3_mixing_open
      msg1_mixing_close_sensor: ecs_s3_mixing_close
      msg1_mixing_open_byte: 1
      msg1_mixing_open_mask: 0xFF
      msg1_mixing_open_value: 0x05
      msg1_mixing_close_byte: 1
      msg1_mixing_close_mask: 0xFF
      msg1_mixing_close_value: 0x06

    - address: POOL310
      msg1_active_sensor: pool310_heating_request
      msg1_valve_request_sensor: pool310_valve_request
      msg1_accessory_sensor: pool310_accessory_active

    - address: SCA35
      msg1_pump_request_sensor: sca35_pump_request

    - address: DEH310
      msg1_pump_request_sensor: deh310_pump_request

    - address: COOLING
      msg1_pump_request_sensor: cooling_pump_request
      msg1_valve_request_sensor: cooling_valve_request
      msg1_active_sensor: cooling_active
```

Observed accessory families:

```text
SCA35  = circulator only
DEH310 = circulator only
POOL310 = active/heating request + reversing/diverter valve + accessory flag
COOLING = circulator + reversing valve + active state
```

The older generic mapping is still supported:

```yaml
msg1_binary_sensor: my_binary_sensor
msg1_binary_byte: 0
msg1_binary_mask: 0x01
```

## COOLING 0x2B

Confirmed from `01_froid_de_off_a_on.txt`:

```text
00 00 = all off
04 00 = cooling active, pump off, inversion valve off
05 00 = cooling active, pump on, inversion valve off
07 00 = cooling active, pump on, inversion valve on
07 29 = same byte0 state with temporary byte1 command/pulse
```

## Optional ACCESSORY constants

The following `ACCESSORY` value is confirmed for ECS_S3 and can be tested for similar accessories:

```yaml
constants:
  - address: ECS_S3
    token: ACCESSORY
    data: [0x01, 0x0F, 0xA2]

  - address: POOL310
    token: ACCESSORY
    data: [0x01, 0x0F, 0xA2]

  - address: SCA35
    token: ACCESSORY
    data: [0x01, 0x0F, 0xA2]

  - address: DEH310
    token: ACCESSORY
    data: [0x01, 0x0F, 0xA2]
```

For `0x55`, `0x90`, and `0xA0`, prefer the dynamic `ecs:` block rather than constants for the same address.

## Debug logging

Recommended during tests:

```yaml
logger:
  level: DEBUG
  logs:
    nibegw.ecs: DEBUG
```

The `0x90` log prints all 8 words:

```text
ECS 0x90 addr=0x6 W0=706 W1=1023 W2=688 W3=1023 W4=1023 W5=1023 W6=1023 W7=1023
```
