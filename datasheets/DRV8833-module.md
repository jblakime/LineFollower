# DRV8833 motordrivermodule (2 kanalen)

Breakout-module met de TI DRV8833 (soms geleverd met de compatibele HR8833 [3]). De chipdatasheet is van Texas Instruments [1]; de pinout hieronder is die van de **module**.

## Pinout module

| Pin | Naam | Functie | Aansluiting in dit project |
|---|---|---|---|
| 1 | EEP | Sleep-ingang (nSLEEP): HIGH = actief, LOW = slaapstand | D8 (`PIN_SLEEP`) |
| 2 | OUT1 | Uitgang H-brug A, 1 | Linkermotor |
| 3 | OUT2 | Uitgang H-brug A, 2 | Linkermotor |
| 4 | OUT3 | Uitgang H-brug B, 1 | Rechtermotor |
| 5 | OUT4 | Uitgang H-brug B, 2 | Rechtermotor |
| 6 | ULT | Fault-uitgang (open drain), LOW bij overstroom/oververhitting | niet aangesloten |
| 7 | IN1 | Ingang, stuurt OUT1 | D5 (PWM) |
| 8 | IN2 | Ingang, stuurt OUT2 | D6 (PWM) |
| 9 | VCC | Motorvoeding (VM) | Batterij+ (2S 18650) |
| 10 | GND | Massa | GND |
| 11 | IN3 | Ingang, stuurt OUT3 | D9 (PWM) |
| 12 | IN4 | Ingang, stuurt OUT4 | D10 (PWM) |

Pinnummering en functies volgens [2] en [3].

> Let op: sommige bronnen verwisselen de betekenis van EEP en ULT. TinyTronics [3] en Components101 [2] geven EEP = sleep en ULT = fault.
> Op de module zit soms een soldeerbrug **J2** die de chip standaard uit slaapstand houdt. Wil je EEP vanuit de Nano sturen, dan moet J2 open zijn [3].

## Specificaties

| Parameter | Waarde | Bron |
|---|---|---|
| Motorvoeding VM (chip) | 2,7 – 10,8 V | [1] |
| Motorvoeding (module) | 3 – 10 V | [3] |
| Ingangsspanning logica (xIN, nSLEEP) | –0,3 – 5,75 V (5 V-logica van de Nano is OK) | [1] |
| Uitgangsstroom per brug | 1,5 A RMS, 2 A piek (module: tot 500 mA aangeraden i.v.m. warmte) | [1], [3] |
| Beveiligingen | overstroom, kortsluiting, oververhitting, onderspanning (UVLO ≈ 2,6 V) | [1] |
| Ingangen | interne pull-down | [1] |

Controle voor dit project: 2S 18650 volledig geladen = 8,4 V < 10,8 V. De N20-motoren vragen hooguit enkele honderden mA (zie `JGA12-N20.md`).

## Waarheidstabel (per brug)

| xIN1 | xIN2 | Functie |
|---|---|---|
| 0 | 0 | Vrijloop (coast) |
| PWM | 0 | Vooruit, snelheid via PWM |
| 0 | PWM | Achteruit, snelheid via PWM |
| 1 | 1 | Remmen |

Volgens de logicatabel in [1].

## Bronnen

1. Texas Instruments – DRV8833 Dual H-Bridge Motor Driver datasheet: https://www.ti.com/lit/ds/symlink/drv8833.pdf
2. Components101 – DRV8833 Dual H-Bridge Motor Driver Module (pinout): https://components101.com/node/2317
3. TinyTronics – DRV8833 Bipolaire Stappenmotor- en DC-Motoraansturing: https://www.tinytronics.nl/nl/mechanica-en-actuatoren/motoraansturingen-en-drivers/stappen-motoraansturingen-en-drivers/drv8833-bipolaire-stappenmotor-en-dc-motor-motoraansturing
