# VibeFlow

**Edge AI vibration monitoring and classification on STM32H533RE + µT-Kernel 3.0**

VibeFlow measures vibration with an GY-86 accelerometer, extracts features with an FFT, and classifies the vibration level entirely on the microcontroller using an INT8-quantized neural network (X-CUBE-AI). No cloud or network connection is needed. A web dashboard shows results and records datasets over USB serial.

| Label | Class | Meaning |
|-------|-------|---------|
| 0 | `stationary` | No vibration |
| 1 | `low_vibration` | Light vibration |
| 2 | `high_vibration` | Strong vibration |

```
GY-86 --I2C--> STM32H533RE --USB serial (115200)--> Laptop --> Web dashboard (Chrome/Edge)
```

## Demo
[![VibeFlow demo video](https://img.youtube.com/vi/G3af8MEP-F0/maxresdefault.jpg)](https://youtu.be/G3af8MEP-F0)

## Features

- On-device FFT feature extraction and neural network inference
- Two modes: **Recording** (labeled CSV datasets) and **Inference** (class + confidence)
- Web dashboard using the Web Serial API, or any serial terminal
- Complete training pipeline (EDA, Random Forest baseline, TinyML MLP)
- Runs on µT-Kernel 3.0 with separate sensor, DSP, and control tasks

## Repository Structure

| Folder / File | Description |
| --- | --- |
| `application/` | Sensor driver, ring buffer, AI inference wrapper, main app logic |
| `Core/`, `Drivers/` | STM32CubeMX-generated code and HAL/CMSIS drivers |
| `mtk3_bsp2/` | µT-Kernel 3.0 board support package |
| `Middlewares/ST/AI/`, `X-CUBE-AI/App/` | X-CUBE-AI runtime and generated model code |
| `Training/` | Notebooks and datasets for model training |
| `dashboard/` | Web dashboard (`index.html`, `script.js`, `styles.css`) |
| `Docs/USER_MANUAL.md` | Full user manual |
| `VER2.ioc` | STM32CubeMX configuration |

## Quick Start

**Hardware:** STM32H533RE Nucleo with the firmware flashed, MPU6050 (GY-86), USB data cable.
**Software:** Chrome or Edge, Python 3 (or any local HTTP server), STM32CubeIDE to build the firmware.

**Wiring**

| MPU6050 | STM32H533RE |
|---------|-------------|
| VCC | 3.3 V |
| GND | GND |
| SCL | PB6 (I2C1 SCL) |
| SDA | PB7 (I2C1 SDA) |

**Build and flash:** open the project in STM32CubeIDE, build, and flash via ST-LINK.

**Run the dashboard**

1. Connect the board over USB.
2. Start a local server in the project folder:
   ```bash
   python -m http.server 5500
   ```
3. Open `http://localhost:5500/dashboard/index.html` in Chrome or Edge.
4. Click **Choose port**, select the board, keep 115200 baud, and connect.
5. Put the board on a flat, still surface and press the **RESET** button.
6. Wait about 10 seconds for calibration, then choose **Start recording** or **Start inference**.

> Connect to the port *before* pressing RESET. Calibrate on the same surface where the device will be used.

## User Manual

The full manual is in [`Docs/USER_MANUAL.md`](Docs/USER_MANUAL.md). Highlights:

- **Recording a dataset:** pick a class, start, keep one vibration level per session, stop, and download the CSV right away (sessions are lost when the tab is closed). One row is produced about every 1.28 s.
- **Running inference:** a new result (class and confidence) appears about every 1.28 s, each covering a 2.56 s window.
- **Serial terminal:** use 115200 8N1. Press `r` to record (then `0`/`1`/`2`/`3` for the class), `i` for inference, `s` to stop.
- **Output format:** lines starting with `#` are status messages; recording rows are CSV; inference results are JSON, e.g. `{"class":"low_vibration","confidence":0.93}`.
- **Troubleshooting:** empty port list, stuck calibration, scrambled output, and other common problems are covered in the manual. Recovery order: press RESET, re-seat wiring, close other serial programs, then clean, rebuild, and re-flash.

## Model and Training

The device runs a small MLP (6 → 16 → 8 → 3, INT8 TFLite) on six features per 256-sample frame: `peak_hz`, `peak_mg`, `rms_mg`, `band_0_5_mg`, `band_5_15_mg`, `band_15_30_mg`.

Run the notebooks in `Training/` in order:

1. `Phase_1_EDA.ipynb`: exploration and cleaning
2. `phase_2_random_forest_baseline.ipynb`: baseline (about 99 % accuracy on test sessions)
3. `phase_3_tinyml_mlp.ipynb`: MLP training, INT8 quantization, scaler export

Always split by session, never by frame. Two datasets are included: `data_1` (controlled vibration, used for the deployed model) and `data_2` (vehicle detection, needs more data).

## Limitations

- 100 Hz sampling, so only vibration up to about 50 Hz is analyzed
- Only the Z axis is used
- Classes reflect vibration level relative to the mounting surface, not absolute strength or vibration source
- Only the three trained classes are recognized; record new data and retrain for new setups

## License

ST components and X-CUBE-AI are licensed under SLA0044. See [`LICENSE_X-CUBE-AI.txt`](LICENSE_X-CUBE-AI.txt).
