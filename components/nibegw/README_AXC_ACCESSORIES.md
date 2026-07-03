# NibeGW - complementary AXC/accessory emulation work

## Remerciements

Ce travail est base sur le composant ESPHome `nibegw` et sur le travail de son auteur original.
Merci a l'auteur et aux contributeurs du projet `esphome-nibe` pour la passerelle NibeGW, le support UDP/RS485, l'emulation RMU et la base qui permet aujourd'hui d'experimenter avec les accessoires Nibe.

Cette branche ne remplace pas le projet d'origine. Elle ajoute un travail complementaire autour de l'emulation d'accessoires de type AXC/ECS afin de tester des circuits climatiques, circulateurs, sondes et vannes depuis ESPHome et Home Assistant.

## Objectif

L'objectif est d'emuler certains accessoires Nibe sur le bus NibeGW, avec des valeurs provenant de Home Assistant:

```text
ECS_S3  = 0x03
POOL310 = 0x06
SCA35   = 0x0A
DEH310  = 0x27
COOLING = 0x2B
```

Le but principal est de pouvoir:

- envoyer les temperatures/valeurs raw attendues par la PAC via le token `0x90`;
- recuperer les demandes de la PAC via le token `0x55`;
- publier ces demandes dans Home Assistant en `binary_sensor`;
- preparer le decodage des commandes de vanne de melange.

## Modifications ajoutees

### Bloc YAML `ecs:`

Le composant `nibegw` accepte maintenant un bloc `ecs:` pour declarer un accessoire dynamique:

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

Chaque entree `ecs:` cree une reponse dynamique au token `0x90`.

### Reponse dynamique `0x90`

La reponse `0x90` contient 8 mots raw little-endian:

```text
W0 W1 W2 W3 W4 W5 W6 W7
```

Mapping YAML:

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

Les valeurs peuvent venir de `sensor.homeassistant`, par exemple des `input_number`.
Le composant garde aussi un cache de la derniere valeur valide recue afin de continuer a repondre a la PAC.

### Etats PAC vers Home Assistant

Le token `0x55` est ecoute et peut publier des etats vers Home Assistant:

```yaml
msg1_pump_request_sensor: sca35_pump_request
msg1_valve_request_sensor: pool310_valve_request
msg1_active_sensor: pool310_heating_request
msg1_accessory_sensor: pool310_accessory_active
msg1_mixing_open_sensor: ecs_s3_mixing_open
msg1_mixing_close_sensor: ecs_s3_mixing_close
```

Les masques generiques sont configurables:

```yaml
msg1_pump_request_mask: 0x01
msg1_valve_request_mask: 0x02
msg1_active_mask: 0x04
msg1_accessory_mask: 0x08
```

Un masque peut etre mis a `0x00` pour desactiver un decodage sur un accessoire donne.
C'est utile pour `ECS_S3`, car la trame `0x55 = 02 00` apparait souvent et ne doit pas forcement etre interpretee comme une vanne d'inversion.

### Preparation vanne de melange

Deux sorties sont prevues pour la vanne de melange:

```yaml
msg1_mixing_open_sensor: ecs_s3_mixing_open
msg1_mixing_close_sensor: ecs_s3_mixing_close
```

Le decodage est configurable:

```yaml
msg1_mixing_open_byte: 1
msg1_mixing_open_mask: 0xFF
msg1_mixing_open_value: 0x05
msg1_mixing_close_byte: 1
msg1_mixing_close_mask: 0xFF
msg1_mixing_close_value: 0x06
```

Ces valeurs sont une preparation de travail. Elles devront etre confirmees avec un dump reel ou la PAC commande explicitement l'ouverture et la fermeture de la vanne de melange.

## Observations issues des dumps

### ECS / AXC

Pour `ECS_S3`:

```text
W0 = BT2
W1 = BT3
W2 = BT50
```

La trame `0x55 = 02 00` est frequente. Elle est conservee dans les logs, mais le decodage `valve_request` peut etre desactive par YAML.

### POOL310

Pour `POOL310`:

```text
W0 = BT51 retour piscine
W1 = inutilise / 1023
W2 = BT59 depart piscine
```

Etats observes sur `0x55`:

```text
00 00 = off
0C 00 = demande piscine active + flag accessoire
0E 00 = demande piscine active + vanne inversion + flag accessoire
```

Interpretation actuelle:

```text
0x02 = vanne inversion
0x04 = demande active / chauffage piscine
0x08 = accessoire actif / flag accessoire
```

### SCA35

Pour `SCA35`:

```text
W0 = BT53 temperature panneau solaire
W1 = BT54 temperature charge solaire
W2 = EP30-BT51 / troisieme sonde selon configuration
```

Un passage `0x55 = 01 00` a ete observe lorsque la temperature solaire monte.
Interpretation actuelle:

```text
0x01 = demande solaire / circulateur probable
```

### DEH310

Pour `DEH310`:

```text
W0 = BT52 chaudiere
W1 = 1023
W2 = 1023
```

Dans les dumps disponibles, la demande circulateur n'a pas encore ete confirmee: `0x55` restait a `00 00`.

### COOLING

Pour `COOLING`:

```text
04 00 = refroidissement actif
05 00 = refroidissement actif + circulateur
07 00 = refroidissement actif + circulateur + vanne inversion
```

Interpretation actuelle:

```text
0x01 = circulateur
0x02 = vanne inversion
0x04 = demande active
```

## Exemple Home Assistant

```yaml
binary_sensor:
  - platform: template
    id: pool310_heating_request
    name: "POOL310 demande chauffage"

  - platform: template
    id: pool310_valve_request
    name: "POOL310 vanne inversion"

  - platform: template
    id: pool310_accessory_active
    name: "POOL310 accessoire actif"

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

Dans Home Assistant, ces sorties apparaissent comme des `binary_sensor`.
`on` correspond a une demande active, `off` a l'absence de demande.

## Etat du travail

Ce travail reste experimental.
Les points deja utiles:

- reponse dynamique `0x90`;
- valeurs raw depuis Home Assistant;
- cache des dernieres valeurs valides;
- decodage et publication des etats `0x55`;
- support de plusieurs accessoires en parallele.

Points encore a confirmer:

- commandes exactes d'ouverture/fermeture de vanne de melange ECS/AXC;
- bit exact de circulateur DEH310 en condition reelle;
- variations completes de `0xA0` selon les modes;
- comportement exact de certains mots `W3..W7`.

Les dumps reels restent la source principale pour continuer a documenter le protocole.
