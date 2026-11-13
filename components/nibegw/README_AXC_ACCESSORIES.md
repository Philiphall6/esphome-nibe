# NibeGW AXC/Accessory Emulation Notes

Maintained by [Philiphall6](https://github.com/Philiphall6). Protocol findings updated 2026-11-12.

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
- decode ECS/AXC mixing valve direction commands using the real S2 capture findings below.

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

These are raw NTC sensor values, **not degrees Celsius**. In the synchronized S2 captures, warmer BT2 water corresponded to a lower W0 value. Do not feed temperatures in degrees directly into these raw fields.

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

### ECS Mixing Valve Commands: Corrected Mapping

Real S2 demand-change captures on 2026-11-12 support direction decoding from
**byte 0**, with bit `0x04` for opening and bit `0x02` for closing:

```yaml
# Fields inside an emulated ECS entry; binary_sensor IDs must exist.
msg1_mixing_open_sensor: ecs_s3_mixing_open
msg1_mixing_close_sensor: ecs_s3_mixing_close
msg1_mixing_open_byte: 0
msg1_mixing_open_mask: 0x04
msg1_mixing_open_value: 0x04
msg1_mixing_close_byte: 0
msg1_mixing_close_mask: 0x02
msg1_mixing_close_value: 0x02
# Disable the generic diverter-valve interpretation for ECS.
msg1_valve_request_mask: 0x00
```

The former byte-1 comparisons `0x05` / `0x06` were hypotheses, not validated
ECS direction commands. Generic code defaults are retained for compatibility;
configure the explicit ECS masks above. Byte 1 varied independently of direction
(`77`, `00`, `5C`, `59` in the controlled tests).

| Byte 0 observed with bit `0x08` set | Direction command | French HA label |
| --- | --- | --- |
| `08` | Neither direction requested | Veille |
| `0A` | Closing (`0x02`) | Fermeture |
| `0C` | Opening (`0x04`) | Ouverture |
| `0E` | Both direction bits set: conflict, not a normal direction | Conflit |

`0E` is the decoder's conflict case, not a state observed in these S2 tests.
These are **commands**, not measured valve position or independently measured
relay outputs. Veille does not mean fully closed. Commands arrive as pulses with
idle intervals; the physical valve may already be at an end stop.

Bit `0x08` is a **candidate circulation-pump request**. The real S2 pump stayed
on throughout these captures, so a pump-off transition is still needed to prove
this mapping. Do not transfer the POOL310/COOLING bit meanings to ECS.

## Observations From Dumps

### ECS / AXC: Real S2 and Emulated S3

The installation used a **real NIBE ECS/AXC S2 card** (`0x02`) and an
**ESP-emulated S3 circuit** (`0x03`) with fictitious sensor values from HA helpers.
S3 observations must not be presented as measurements from another physical card.

The existing temperature-word mapping is:

```text
W0 = BT2 raw supply sensor
W1 = BT3 raw return sensor
W2 = BT50 raw room sensor
```

The historic S3 frame `02 00` contains the closing bit under the corrected ECS
mapping. It should not be decoded as a generic reversing/diverter valve request.

#### Opposite demand changes on real S2

All times below are UTC, 2026-11-12. Bus and HA samples have independent polling
timestamps; compare trends rather than assuming identical sampling instants.

| Test | Recording window | HA calculated supply | HA supply trend | Bus result |
| --- | --- | --- | --- | --- |
| Lower demand, offset settling at −10 | 22:00:35–22:10:35 | 31.4 → 25.0 °C | 33.4 → 29.8 °C | Four `0A` closing pulses, approximately 9, 8, 7 and 6 seconds; no `0C` |
| Raise demand, offset −10 → +3 | 22:14:08–22:20:03, initial snapshot | 25.0 → 36.0 °C | 29.0 → 28.4 → 30.4 °C | Two `0C` opening pulses, approximately 8 seconds each; no `0A` |

The closing capture contains 600 valid temperature responses and 600 commands;
the opening snapshot contains 356 valid temperature responses and 355 commands.
W0 increased from 613 to 649 while the water cooled in the closing test. During
the opening snapshot it reached 666, then fell to 639 as the water warmed.

[Capture evidence and synchronized HA samples](../../docs/captures/2026-11-12-s2-valve-directions.json)
record the counts, UTC transitions and limitations. The opening evidence file is
explicitly a partial snapshot, not the full ten-minute recording.

#### Token `0xA0`

Observed two-byte values behaved like a big-endian controller index: for example,
`003C → 003B` (60 → 59) during a closing pulse. On emulated S3, the index rose
95 → 99 → 100, then stayed at 100 while opening pulses continued. This suggests
a calculated valve index; it does **not** establish physical position feedback
or a measured opening percentage.

#### Passive observation and readable states

Never add the real S2 address to `acknowledge` or make an emulator reply in place
of its physical card. Passive observation, transition logs, and the three-state
HA text sensor are implemented on the
[`ecs-s2-passive-diagnostics` branch](https://github.com/Philiphall6/esphome-nibe/tree/ecs-s2-passive-diagnostics),
not in the code on `master` at the time of this documentation update.

Use that branch's
[passive S2 / emulated S3 example](https://github.com/Philiphall6/esphome-nibe/blob/ecs-s2-passive-diagnostics/examples/ecs-s2-observe-s3-emulate.yml)
for `observe_only: true` and French text states. Its ECS INFO logs report
`valve_command=opening`, `closing` or `idle`, plus the configured pump-request
interpretation. A `pump_request=ON` log does not independently prove bit `0x08`.

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

- ECS circulation-pump bit with an actual pump-off transition;
- physical relay-output verification and applicability beyond the tested installation;
- exact DEH310 GP15 pump bit under a real heat-demand condition;
- full `0xA0` variations depending on operating modes;
- exact meaning of some `W3..W7` words.

Real accessory dumps remain the primary source for documenting and validating the protocol.
