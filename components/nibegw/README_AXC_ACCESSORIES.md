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

Each active `ecs:` entry creates a dynamic reply for token `0x90`. An entry
with `observe_only: true` only observes the real accessory.

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
msg1_mixing_open_byte: 0
msg1_mixing_open_mask: 0x04
msg1_mixing_open_value: 0x04
msg1_mixing_close_byte: 0
msg1_mixing_close_mask: 0x02
msg1_mixing_close_value: 0x02
```

These ECS direction bits are supported by synchronized S2/S3 captures and opposite demand changes on real S2 on 2026-11-12 (see the three-state decoder section below). They describe requested direction, not measured physical position or independently probed relay outputs.

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

- independent physical verification of the inferred ECS/AXC direction bits;
- exact DEH310 GP15 pump bit under a real heat-demand condition;
- full `0xA0` variations depending on operating modes;
- exact meaning of some `W3..W7` words.

Real accessory dumps remain the primary source for documenting and validating the protocol.

## Real S2 card: passive diagnostics before S3 emulation

Use `observe_only: true` on an `ecs:` entry when a real AXC/ECS card already
occupies its address. This registers listeners for controller messages `0x55`
and `0xA0` but **does not ACK or provide a `0x90` reply**. The validator rejects
an observer address also present in `acknowledge`, `constants`, or UDP command
ports. Duplicate `ecs:` addresses are rejected as well. Other gateway functions
(such as MODBUS40) continue normally.

`observe_only` defaults to `false` to preserve existing emulators. Never enable
emulation at an address occupied by a real card.

Optional `msg1_raw_sensor` and `msg2_raw_sensor` reference template text sensors.
They publish the complete decoded payload as hexadecimal bytes, without guessing
its meaning. These are commands from the controller, **not valve position or
proof that the physical circulator is running**. Short `0x55` payloads are exposed
as raw diagnostics but do not overwrite binary command states.

See [`examples/ecs-s2-observe-s3.yml`](../../examples/ecs-s2-observe-s3.yml) for a
package to merge into an existing gateway. Both entries start passive. If S3 is
already emulated, merge the two raw sensor references into that existing S3 entry
and retain its working temperature inputs, defaults and `observe_only: false`.
Do not create a second S3 entry. No relay outputs are configured by this package.

For a new setup matching a real S2 and an emulated S3, use
[`ecs-s2-observe-s3-emulate.yml`](../../examples/ecs-s2-observe-s3-emulate.yml)
instead. It extends the passive package, retains the documented S3 raw sensor
inputs and accessory identity, and exposes its existing experimental pump/open/close
decoder. Replace input IDs/defaults with the working installation values; do not
add this package alongside an already configured S3 entry.
Validate this variant with `esphome config .ci/ecs-emulator.yaml`.

### Historical status before the synchronized long capture

Historical captures contain S2 payloads `02 77`, `02 00`, `02 59` and escaped
bytes; S3 contains `02 00` and `02 06`. No synchronized physical valve/pump
observation accompanies those captures. In particular, **byte 0 mask `0x01` and
byte 1 values `0x05`/`0x06` remain hypotheses for ECS**, not confirmed S2 relay
assignments. Do not connect the existing decoded binary sensors to relay outputs
on that basis. Generic masks for other accessories are not ECS evidence.

Observe S2 through a normal heating cycle. Record the timestamp, both raw
payloads, BT2 S2, calculated supply S2, and independently observed opening,
closing, stationary valve and circulator on/off states. Compare transitions and
repeats across several cycles. Only then configure `msg1_*_mask` and
`msg1_mixing_*` for the established mapping and replay it on an unoccupied S3.
An `ON` command is not an opening percentage; estimating position would additionally
require verified travel time and a known reference position.

Validation: `python3 -m unittest discover -s tests -v` (requires `g++`);
`esphome config .ci/ecs-observer.yaml` and `esphome compile .ci/ecs-observer.yaml`.

### Live comparison (2026-11-12)

Read-only capture from the already running gateway, without changing the NIBE
settings or flashing the ESP, produced the following controller payloads:

| Circuit | `0x55` | `0xA0` |
| --- | --- | --- |
| Real S2 AXC | `08 77` | `00 3E` |
| Emulated S3 | `08 00`, `08 04`, `08 5D` | `00 43` |

The ESP log also contained S3 `08 77`. Existing S3 temperature replies were
working (W0=740, W1=700, W2=700). These observations preserve the established
sensor work, but do not establish valve direction or pump relay mapping.
In particular the existing `pump_request=OFF` log is the result of applying
mask `0x01`; it is not independent confirmation that the circulator is off.
The `0xA0` payload is nonzero in this capture and must not be discarded.

The software observer and diagnostic exposure are implemented and compiled;
physical ECS relay decoding remains unvalidated. Do not describe this as a
completed physical AXC replacement yet.

### Synchronized real S2 capture, 20:53:35–20:56:34 UTC

On 2026-11-12, a three-minute passive UDP recording included **180 real S2
`0x90` replies**, 180 `0x55` commands and 180 `0xA0` commands. Master and slave
checksums were validated before decoding; no invalid/missing S2 replies were
found. The slave `C0 90 10 ... checksum` frame follows the master request in the
same UDP datagram. Reading only the master payload would miss the temperatures.
Payload `5C 5C` escapes were removed before interpreting little-endian words.

| Raw word | First | Last | Range |
| --- | ---: | ---: | ---: |
| W0 / BT2 | 633 | 630 | 630–633 |
| W1 / BT3 | 682 | 680 | 680–682 |
| W2 / BT50 | 739 | 728 | 718–740 |

These are raw sensor values, not degrees Celsius. The BT labels follow the
fork's established mapping; this capture alone does not recalibrate the sensors.

`0x55` payload counts: `08 77` ×170, `08 5C` ×4, `08 00` ×4, `0A 77` ×2.
At 20:54:45.663, byte 0 changed from `08` to `0A`, then returned to `08`
at 20:54:47.657. W0 stayed at 631 during that approximately two-second interval.
It eventually reached 630 by the end of the recording. This is insufficient to
attribute the subsequent temperature change to a specific valve direction.

Working hypothesis for ECS only: persistent byte-0 bit `0x08` may be the
circulator request, while the transient `0x02` may be a valve command. **Neither
assignment is confirmed.** Other functions can also stay active throughout this
short capture. A pump-off observation and independent valve-direction evidence
are still needed; the defaults have deliberately not been remapped from these
observations alone. Byte 1 changes independently, so equality with `05`/`06`
remains unproven as an opening/closing decoder.

## ECS valve decoder: opening, closing, idle (2026-11-12 update)

Synchronized S2/S3 observations, followed by opposite demand changes on the
real S2, support the following **command-direction** mapping. Use explicit options;
the generic component defaults remain unchanged for other accessory families.

| `0x55` byte 0 (pump bit present) | Valve action | Direction mask |
| --- | --- | --- |
| `08` | idle (neither direction energized) | `byte0 & 06 == 00` |
| `0C` | opening | `byte0 & 06 == 04` |
| `0A` | closing | `byte0 & 06 == 02` |
| `0E` | conflicting direction bits | `byte0 & 06 == 06` |

Evidence:
- On emulated S3, `08 -> 0C -> 08` pulses last about five seconds and recur
  about every 146 seconds. `0xA0` progresses `00 5F -> 00 63 -> 00 64`
  (95 -> 99 -> 100) at the first pulses. Its emulated sensor inputs stay fixed.
- On real S2, the short capture shows `08 -> 0A -> 08` for about two seconds,
  coinciding with `0xA0` decreasing `00 3C -> 00 3B` (60 -> 59).
- Byte 1 varies even while the valve direction bits remain idle. Matching
  byte 1 against `05` or `06` does not describe these observed transitions.

This supports the direction mapping and suggests `0xA0` byte 1 is a calculated
valve position/index. It is **not measured physical position feedback**.
Byte-0 bit `08` remains a pump candidate, consistent with NIBE register 44746
remaining at 1 and the user's confirmation that the circulator stays on. A
pump-off transition has not been captured, so that assignment is less certain.

`msg1_mixing_state_sensor` references a template text sensor and publishes:
`opening`, `closing`, `idle`, or `conflict`. If either direction mask is disabled,
it publishes `unknown`. Truncated frames do not replace the last valid state.
`idle` means neither direction is requested, not that the valve is fully closed.

The updated observer/emulator examples use byte-0 masks `04` and `02` and
expose this state. The S3 example explicitly selects pump mask `08` as the
above working hypothesis. No GPIO relay control is configured, and no live
firmware or heating setting was changed during the initial captures. In the
later demand-change tests below, the user changed the S2 offset through HA.

### Completed 30-minute recording and temperature correlation

Window: 2026-11-12 20:59:55–21:29:55 UTC. 49,286 bus datagrams were saved.
S2 provided 1,800 valid `0x90` responses; no S2 checksum/missing-reply error was
detected by the decoder. There were 1,799 S2 `0x55` commands in the window.

- S2 W0/BT2: 630 -> 620 raw; W1/BT3: 679 -> 666 raw.
- S2 W2/BT50 varied between 717 and 740 raw.
- HA's separately recorded S2 departure rose from 31.6 to 32.6 °C, return
  from 26.7 to 28.1 °C, and calculated departure from 30.7 to 31.9 °C.
  These HA values have their own polling timestamps; they are not an exact
  conversion of each bus sample.
- S2 `0x55` remained byte0=08 throughout (byte1: 77 ×1726, 00 ×40, 5C ×33).
  Its `0xA0` remained `00 3A`. Thus temperature variation alone did **not**
  produce a captured S2 direction change in this window.
- S3's `0x90` inputs stayed fixed at W0=740, W1=700, W2=700. Its `0x55`
  produced 13 approximately five-second `0C` pulses, with `08` between pulses
  (65 opening-command samples and 1,734 idle samples).
- S3 `0xA0` increased from 95 to 99 to 100, then stayed at 100 while repeated
  opening pulses continued. This reinforces that it is not physical feedback.

The long capture supports the opening/idle interpretation, but contains no
S2 closing transition. At this stage, the closing inference rested on the earlier short
S2 capture and the simultaneous decreasing A0 index. Neither valve direction in this initial dataset was
independently verified against real movement or against deliberately
varied S3 temperatures. The user confirms the real circulator remains on; this
capture is not intended to force it off.


### Real S2: opposite demand changes confirm the direction interpretation

The user changed the real S2 heating offset in HA while the observer passively
recorded the bus. S2 uses a real NIBE ECS/AXC board; these are not responses
from the emulated S3. The heating curve stayed at 6 in both test windows.
The [sanitized evidence snapshot](../../docs/captures/2026-11-12-s2-valve-directions.json)
contains checked frame counts, direction transitions and the corresponding HA
history. All times below are **UTC on 2026-11-12**.

**Lower demand, complete 22:00:35–22:10:35 recording:**

- Offset settled at −10; calculated S2 departure changed from 31.4 to 25.0 °C.
- Measured HA departure fell from 33.4 to 29.8 °C.
- Four `08 -> 0A -> 08` pulses began at 22:01:12.514, 22:03:42.475,
  22:06:11.430 and 22:08:39.383. Durations were approximately 9, 8, 7 and
  6 seconds respectively (30 command samples in total).
- No `0C` was observed. There were 600 valid temperature responses and no
  checksum/missing-response errors reported by the S2 decoder.
- W0/BT2 changed from 613 to 649 raw as the water cooled. The raw NTC value
  must not be interpreted directly as degrees Celsius.

**Raise demand, initial 22:14:08–22:20:03 snapshot:**

- Offset increased from −10 and ended at +3 in this snapshot; calculated S2
  departure rose from 25.0 to 36.0 °C.
- HA departure initially fell from 29.0 to 28.4 °C, then rose to 30.4 °C.
- Two `08 -> 0C -> 08` pulses began at 22:15:57.269 and 22:18:26.221,
  each lasting approximately 8 seconds (16 command samples in total).
- No `0A` was observed. The snapshot contains 356 valid temperature
  responses and no S2 checksum/missing-response errors.
- W0/BT2 was 663 initially, reached 666, and then fell to 639 raw.
  The ten-minute recording was still running at this snapshot's cutoff.

Together, these opposite demand tests strongly support **bit 0x02 = closing,
bit 0x04 = opening, neither bit = idle** in byte 0 of the S2 `0x55`
payload. Byte 1 varied within an unchanged direction, including `0C 59`,
`0C 5C` and `0C 77`; it is not a direction selector.

The circulator state (register 44746) remained 1 in both experiments. Thus the
pump assignment to bit 0x08 remains unverified by an on/off transition.
An offset of −10 does not itself prove movement: the controller issues
direction pulses with pauses, and a valve already at its end stop may not move.
These observations confirm the **command interpretation**, not valve position,
travel completion or measured relay voltage. The existing explicit ECS masks
already match the evidence; no decoder or generic-default change is needed.
