# NibeGW AXC/Accessory Emulation Notes

## Acknowledgements

This work is based on the ESPHome `nibegw` component and on the original author's work.
Thank you to the author and contributors of `esphome-nibe` for the NibeGW gateway, UDP/RS485 support, RMU emulation, and the foundation that makes this accessory experimentation possible.

This branch does not replace the original project. It adds complementary experimental work around AXC/ECS-style accessory emulation, with the goal of testing climate circuits, pumps, sensors, reversing valves, and mixing valves from ESPHome and Home Assistant.

## Goal

The goal is to emulate selected Nibe accessories on the NibeGW bus, using values provided by Home Assistant:

```text
ECS_S3  = 0x03
POOL310 = 0x06
SCA35   = 0x0A
DEH310  = 0x27
COOLING = 0x2B
```

The main objectives are:

- send the raw temperature/sensor values expected by the heat pump through token `0x90`;
- receive heat-pump commands and states from token `0x55`;
- expose these commands and states to Home Assistant as ESPHome `binary_sensor` entities;
- prepare configurable decoding for ECS/AXC mixing valve open/close commands.

## Added Features

### YAML `ecs:` Block

The `nibegw` component now accepts an `ecs:` block to declare dynamic accessory emulators:

```yaml
nibegw:
  id: my_nibegw

  acknowledge:
    - ECS_S3
    - POOL310
    - SCA35
    - DEH310

  ecs:
    - address: ECS_S3
      bt2_raw: s3_bt2_raw
      bt3_raw: s3_bt3_raw
      bt50_raw: s3_bt50_raw
```

Each `ecs:` entry creates a dynamic reply for token `0x90`.

### Dynamic `0x90` Response

The `0x90` response contains 8 little-endian raw 16-bit words:

```text
W0 W1 W2 W3 W4 W5 W6 W7
```

YAML mapping:

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

Values can come from ESPHome `sensor.homeassistant` entities, for example Home Assistant `input_number` helpers.
The component also caches the last valid value received from Home Assistant so it can keep replying to the heat pump if a value is not immediately available.

### Heat Pump States to Home Assistant

Token `0x55` is listened to and can publish decoded states to Home Assistant:

```yaml
msg1_pump_request_sensor: sca35_pump_request
msg1_valve_request_sensor: pool310_valve_request
msg1_active_sensor: pool310_heating_request
msg1_accessory_sensor: pool310_accessory_active
msg1_mixing_open_sensor: ecs_s3_mixing_open
msg1_mixing_close_sensor: ecs_s3_mixing_close
```

The generic byte-0 masks are configurable:

```yaml
msg1_pump_request_mask: 0x01
msg1_valve_request_mask: 0x02
msg1_active_mask: 0x04
msg1_accessory_mask: 0x08
```

Set a mask to `0x00` to disable that decoded state for one accessory.
This is useful for ECS/AXC because `0x55 = 02 00` appears frequently on S3 and should not automatically be treated as a reversing valve command.

### Mixing Valve Preparation

Two outputs are prepared for ECS/AXC mixing valve commands:

```yaml
msg1_mixing_open_sensor: ecs_s3_mixing_open
msg1_mixing_close_sensor: ecs_s3_mixing_close
```

The decoding is configurable:

```yaml
msg1_mixing_open_byte: 1
msg1_mixing_open_mask: 0xFF
msg1_mixing_open_value: 0x05
msg1_mixing_close_byte: 1
msg1_mixing_close_mask: 0xFF
msg1_mixing_close_value: 0x06
```

These values are working assumptions. They must be confirmed with a real dump where the heat pump explicitly commands the mixing valve to open and close.

## Observations From Dumps

### ECS / AXC

For `ECS_S3`:

```text
W0 = BT2
W1 = BT3
W2 = BT50
```

The frame `0x55 = 02 00` appears frequently.
It is still logged, but the generic `valve_request` decoding can be disabled in YAML with:

```yaml
msg1_valve_request_mask: 0x00
```

### POOL310

For `POOL310`:

```text
W0 = BT51 pool return temperature
W1 = unused / 1023
W2 = BT59 pool supply temperature
```

Observed `0x55` states:

```text
00 00 = off
0C 00 = pool active/heating request + accessory flag
0E 00 = pool active/heating request + reversing valve + accessory flag
```

Current interpretation:

```text
0x02 = reversing/diverter valve
0x04 = active request / pool heating request
0x08 = accessory active / accessory flag
```

### SCA35

For `SCA35`:

```text
W0 = BT53 solar panel temperature
W1 = BT54 solar load temperature
W2 = EP30-BT51 / third solar sensor depending on configuration
```

A transition to `0x55 = 01 00` was observed when the solar temperature increased.
Current interpretation:

```text
0x01 = solar request / likely circulation pump request
```

### DEH310

For `DEH310`:

```text
W0 = BT52 boiler temperature
W1 = 1023
W2 = 1023
```

Additional observation:

```text
0x55 byte 0 mask 0x08 = active state also observed on DEH310
```

As with POOL310, `0x08` should be treated carefully.
It may be an accessory-active flag or a request related to the accessory depending on the operating context.

Recommended first mapping:

```yaml
msg1_accessory_sensor: deh310_accessory_active
```

If a later dump confirms that `0x08` directly represents the GP15 charge pump command, it can also be mapped as:

```yaml
msg1_pump_request_mask: 0x08
msg1_pump_request_sensor: deh310_gp15_request
```

### COOLING

For `COOLING`:

```text
04 00 = cooling active
05 00 = cooling active + pump
07 00 = cooling active + pump + reversing valve
```

Current interpretation:

```text
0x01 = pump
0x02 = reversing valve
0x04 = active request
```

## Home Assistant Example

```yaml
binary_sensor:
  - platform: template
    id: pool310_heating_request
    name: "POOL310 heating request"

  - platform: template
    id: pool310_valve_request
    name: "POOL310 reversing valve"

  - platform: template
    id: pool310_accessory_active
    name: "POOL310 accessory active"

nibegw:
  ecs:
    - address: POOL310
      bt2_raw: pool_bt51_raw
      bt3_raw_default: 1023
      bt50_raw: pool_bt59_raw
      msg1_active_sensor: pool310_heating_request
      msg1_valve_request_sensor: pool310_valve_request
      msg1_accessory_sensor: pool310_accessory_active
```

In Home Assistant, these outputs appear as `binary_sensor` entities.
`on` means the request or state is active, and `off` means it is inactive.

## Current Status

This work is experimental.
Currently useful features:

- dynamic `0x90` response;
- raw values from Home Assistant;
- cache of the last valid raw values;
- `0x55` state decoding and Home Assistant publishing;
- several accessories running in parallel.

Still to confirm:

- exact ECS/AXC mixing valve open/close commands;
- exact DEH310 GP15 pump bit under a real heat-demand condition;
- full `0xA0` variations depending on operating modes;
- exact meaning of some `W3..W7` words.

Real accessory dumps remain the primary source for documenting and validating the protocol.
