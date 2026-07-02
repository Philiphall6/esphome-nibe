# NibeGW ECS / AXC style accessories from Home Assistant values

This component can emulate Nibe accessory data blocks that use token `0x90`.

It was tested with:

```text
ECS_S3  = 0x03
POOL310 = 0x06
SCA35   = 0x0A
DEH310  = 0x27
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

Confirmed circulator request:

```text
token 0x55, byte 0, mask 0x08 = pool pump request
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
    id: pool310_pump_request
    name: "POOL310 demande circulateur"

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
      msg1_binary_sensor: pool310_pump_request
      msg1_binary_byte: 0
      msg1_binary_mask: 0x08
```

When the heat pump requests the pool pump, logs should show:

```text
ECS 0x55 addr=0x6 byte=0 mask=0x08 state=ON
```

## SCA35 and DEH310

These appear to use the same `0x90` format, but the exact useful word(s) and pump bit still need confirmation with active dumps.

Observed idle data:

```text
SCA35  addr=0x0A: W0 around 700, W1 around 699, W2 around 687..703
DEH310 addr=0x27: W0 around 701, W1 around 700, W2 around 700..703
```

In the available idle dumps, `0x55` was:

```text
SCA35  00 00
DEH310 00 00
```

So the circulator bit is not confirmed for SCA35/DEH310 yet. If they behave like POOL310, start by testing:

```yaml
msg1_binary_byte: 0
msg1_binary_mask: 0x08
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

For POOL310, these constants may also be useful:

```yaml
  - address: POOL310
    token: ECS_DATA_MSG_1
    data: [0x02, 0x08, 0x00]

  - address: POOL310
    token: ECS_DATA_MSG_2
    data: [0x02, 0x64, 0x00]
```

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

