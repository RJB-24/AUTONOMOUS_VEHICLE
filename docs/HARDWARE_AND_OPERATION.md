# Hardware, wiring, and operating plan

## Important assessment of the purchase list

The listed parts can make a **vision-assisted obstacle-avoiding robot**, but not a robot that can accurately reach an arbitrary coordinate. There are no wheel encoders, IMU, GPS, LiDAR, or map/localisation sensor. For Assignment 2, define the target as safe forward travel, a visible coloured target, or an ArUco marker. Add two encoder motors (or wheel encoders) and preferably an IMU if pose/coordinate navigation is required.

The Nano cannot run the camera CNN. The ESP32-CAM runs the quantised model; the Nano owns motors and safety sensors. This separation ensures a stalled camera does not leave the motors moving.

## Electrical cautions

1. Determine whether the two-cell holder connects cells in series (nominal 7.4 V, 8.4 V full) or parallel (3.7 V). The L298N needs series voltage for typical 6 V motors; do not guess polarity or topology.
2. Use protected, matched Li-ion cells and a suitable **2S BMS/balanced charger** if the holder is series. The shopping list does not show a charger or BMS. Never charge cells through the LM2596.
3. Set the LM2596 output to 5.0 V with a multimeter **before** connecting electronics. Supply its 5 V output to Nano `5V` and ESP32-CAM `5V`, not `3V3`. It should provide at least 1 A with margin for camera peaks.
4. All controller, sensor, buck, and L298N grounds must be common. Keep motor power wiring away from camera wiring. Add 470–1000 uF near the ESP32-CAM 5 V input and 100 nF across each motor if resets/noise occur.
5. L298N has a large voltage drop and wastes energy. It will work for a prototype if motor stall current is within its real capability. A TB6612FNG is a better replacement.
6. Test with wheels lifted first and provide a physical battery disconnect/emergency switch. The list does not include one.

## Nano wiring

| Part | Signal | Nano |
|---|---|---|
| ESP32-CAM | TX0 / GPIO1 | D2 (SoftwareSerial RX) |
| HC-SR04 | TRIG | D3 |
| HC-SR04 | ECHO | D4 |
| L298N | ENA | D5 (PWM; remove ENA jumper) |
| L298N | ENB | D6 (PWM; remove ENB jumper) |
| Left IR | OUT | D7 |
| Right IR | OUT | D8 |
| L298N | IN1, IN2 | D9, D10 |
| L298N | IN3, IN4 | D11, D12 |

The HC-SR04 and IR modules connect only to the 5 V Nano. ESP32 GPIO is not 5 V tolerant. ESP TX is 3.3 V logic and is normally recognised by the Nano input. No Nano-to-ESP connection is required.

If forward movement is reversed or rotates, swap one motor's wires or change that motor's sign in `motors()` after a wheels-off-ground test.

## ESP32-CAM connection and upload

The source assumes an AI-Thinker-style ESP32-CAM pinout. Confirm the PCB model printed on the board; “OV3660” names the image sensor and does not guarantee the board pin mapping. The listed Nano CH340 can be used as a USB-to-serial bridge only with care (hold Nano RESET low and protect ESP RX from 5 V), but a 3.3 V-compatible USB-UART programmer is strongly recommended.

GPIO1/TX0 connects to Nano D2 during operation. Disconnect this wire while uploading firmware. Boot/upload details vary by board (AI-Thinker commonly requires GPIO0 grounded during reset).

## Sensor mounting

- Camera: 250–300 mm high, initially 15° downward.
- HC-SR04: front centre, level, with unobstructed acoustic cones.
- IR modules: front-left and front-right, angled slightly outward.
- Tune IR potentiometers on the actual floor. Reflective black, transparent, and angled objects can defeat these low-cost sensors.

## Decision policy

The Nano finite-state machine has these priorities:

1. Ultrasonic distance ≤25 cm or both IR sensors active: stop, reverse briefly, pivot.
2. One IR active: pivot away.
3. Missing camera data for 1.2 seconds or confidence below 70%: stop.
4. Otherwise follow the CNN command, slowing below 55 cm.

The `BLOCKED` response rotates to search. This is obstacle avoidance, not global route planning. Speeds and distances must be tuned experimentally.

## Data collection and validation

Use the camera after final mounting. Each label means the safest action:

- `forward`: centre corridor is safe.
- `left`: safest traversable space is left.
- `right`: safest traversable space is right.
- `blocked`: no visible safe route or immediate stop/search is appropriate.

Capture at least 500 diverse images per class for a prototype. Include unfamiliar obstacles and lighting. Split recording sessions—not adjacent video frames—between train/validation/test. Required acceptance tests should include:

- `blocked` recall ≥95% on the held-out indoor test set;
- overall test accuracy ≥90%;
- camera message rate ≥5 Hz;
- 20 runs without collision from obstacles first visible at 1 m;
- fail-safe stop after camera disconnection;
- low-battery and dim/bright-light trials.

ML accuracy alone does not establish safety. Operate at low speed under supervision.
