# Ajout propose pour piloter ECS S3 depuis Home Assistant

Dans ton fork, j'ai ajoute localement :

```text
components/nibegw/NibeGwEcs.h
components/nibegw/NibeGwEcs.cpp
```

et modifie :

```text
components/nibegw/__init__.py
```

## Principe

Le composant ajoute un bloc `ecs:` dans `nibegw:`.

Il repond dynamiquement au token :

```text
address = ECS_S3 / 0x03
token   = ECS_DATA_REQ / 0x90
```

avec :

```text
W0 = BT2 raw
W1 = BT3 raw
W2 = BT50 raw
W3-W7 = 0x03FF
```

Les valeurs attendues sont les valeurs brutes AXC40/NTC, pas des temperatures en degres.

## Exemple ESPHome

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
  id: nibe_gw
  acknowledge:
    - MODBUS40
    - RMU40_S1
    - ECS_S3

  ecs:
    - address: ECS_S3
      bt2_raw: s3_bt2_raw
      bt3_raw: s3_bt3_raw
      bt50_raw: s3_bt50_raw

  constants:
    - address: MODBUS40
      token: ACCESSORY
      data: [0x0A, 0x00, 0x02]

    - address: ECS_S3
      token: ACCESSORY
      data: [0x01, 0x0F, 0xA2]

    - address: ECS_S3
      token: ECS_DATA_MSG_1
      data: [0x02, 0x02, 0x77, 0x20, 0x06]

    - address: ECS_S3
      token: ECS_DATA_MSG_2
      data: [0x02, 0x00, 0x00, 0xA0, 0x06]
```

Ne mets plus de constante `ECS_DATA_REQ` pour `ECS_S3`, car le bloc `ecs:` fournit deja la reponse dynamique `0x90`.

## Exemples de valeurs raw

Valeurs observees dans les dumps :

```text
BT2 autour de 704 -> C0 02
BT3 autour de 724 -> D4 02
BT50 absent       -> 1023 -> FF 03
```

Donc dans Home Assistant, pour demarrer :

```text
input_number.nibe_s3_bt2_raw  = 704
input_number.nibe_s3_bt3_raw  = 724
input_number.nibe_s3_bt50_raw = 1023
```

## Pour une entite number.xxx

`platform: homeassistant` peut lire un etat numerique Home Assistant. Tu peux donc pointer vers :

```yaml
entity_id: number.xxx
```

ou vers :

```yaml
entity_id: input_number.xxx
```

Le composant arrondit ensuite la valeur et la limite entre `0` et `1023`.

