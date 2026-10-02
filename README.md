# Vision-Based Autonomous Mobile Robot

Course project for 20MUPC701 – Robotic Vision and Intelligence.

This repository implements a hybrid controller for the purchased hardware:

- ESP32-CAM + OV3660: camera inference (`LEFT`, `FORWARD`, `RIGHT`, `BLOCKED`)
- Arduino Nano CH340: ultrasonic/IR safety fusion and L298N motor control
- HC-SR04 and two IR modules: independent close-range collision protection
- Two-wheel differential-drive chassis

See [docs/HARDWARE_AND_OPERATION.md](docs/HARDWARE_AND_OPERATION.md) before wiring or powering the robot.

## Architecture

```text
OV3660 -> ESP32-CAM -> int8 CNN -> UART vision message
                                      |
HC-SR04 + two IR sensors -> Arduino Nano FSM -> L298N -> motors
```

The ML result is advisory. The Nano always overrides it when a proximity sensor reports danger or when camera messages become stale.

## Train the model

Use Python 3.10/3.11. Collect images into:

```text
ml/dataset/left/
ml/dataset/forward/
ml/dataset/right/
ml/dataset/blocked/
```

Then:

```bash
python -m venv .venv
. .venv/bin/activate
pip install -r ml/requirements.txt
python ml/train.py --data ml/dataset --epochs 35
python ml/model_to_header.py ml/models/navigation_int8.tflite firmware/esp32_cam_vision/src/model_data.h
```

Do not deploy until the test-set confusion matrix is acceptable, especially recall for `blocked`.

## Firmware

The Nano sketch is in `firmware/nano_controller/nano_controller.ino`. Two camera modes are provided:

1. `firmware/esp32_cam_vision/`: fully onboard ESP32 inference.
2. `firmware/esp32_cam_wifi_bridge/`: recommended laptop mode. The ESP32 serves JPEG snapshots and forwards UDP decisions to the Nano.

### Recommended laptop mode

Copy `wifi_config.example.h` to the ignored `wifi_config.h`, enter the Wi-Fi/hotspot credentials, and upload the bridge firmware. Put the laptop and ESP32 on the same private Wi-Fi network. After training the model, run:

```bash
pip install tensorflow opencv-python numpy
python laptop_controller/controller.py --esp 192.168.1.90 --show
```

Replace the address with the ESP32's assigned IP. `--edge-fallback` enables an experimental Canny-edge heuristic, but it is disabled by default because edges do not reliably represent traversable space. Closing the controller, losing Wi-Fi, low confidence, or stale messages produces a stop command.
