# VibeFlow User Manual

**Vibration monitoring and classification on STM32H533RE + µT-Kernel 3.0**
Version: firmware _(fill in)_ · model _(fill in)_ · dashboard _(fill in)_

---

## Table of Contents

1. [What is VibeFlow?](#1-what-is-vibeflow)
2. [Requirements](#2-requirements)
3. [Wiring](#3-wiring)
4. [Quick Start](#4-quick-start)
5. [Recording a Dataset](#5-recording-a-dataset)
6. [Running Inference](#6-running-inference)
7. [Using a Serial Terminal Instead of the Dashboard](#7-using-a-serial-terminal-instead-of-the-dashboard)
8. [Serial Output Format](#8-serial-output-format)
9. [Troubleshooting](#9-troubleshooting)
10. [Model and Training Data](#10-model-and-training-data)
11. [Limitations](#11-limitations)
12. [Quick Reference](#12-quick-reference)

---

## 1. What is VibeFlow?

VibeFlow measures vibration with an MPU6050 accelerometer (GY-86 module), processes it on an STM32H533RE, and classifies it into three states:

| Label | Class | Meaning |
|-------|-------|---------|
| 0 | `stationary` | No vibration |
| 1 | `low_vibration` | Light vibration |
| 2 | `high_vibration` | Strong vibration |

It is a complete **edge AI** device: the signal processing (FFT and feature extraction) and the neural network run entirely on the microcontroller. No cloud, no laptop-side model, and no network connection are needed to detect and classify vibration. The laptop is only used to display results and save recordings.

It has two modes:

- **Recording**: collects labeled feature data as CSV, for training or re-training the model.
- **Inference**: runs the on-board AI model and reports the current class with a confidence value.

```
MPU6050 (GY) --I2C--> STM32H533RE --USB serial (115200)--> Laptop --> Web dashboard (Chrome/Edge)
```

---

## 2. Requirements

**Hardware**
- STM32H533RE Nucleo board with VibeFlow firmware flashed
- MPU6050 (GY-86) module, wired to the board
- USB cable (data cable, not charge-only)

**Software**
- Google Chrome or Microsoft Edge (Web Serial API is required; Firefox and Safari do not work)
- A way to start a local HTTP server, for example Python 3 (`python -m http.server`) or the VS Code "Live Server" extension
- ST-LINK virtual COM port driver, if your OS does not detect the board automatically

---

## 3. Wiring

| MPU6050 (GY) | STM32H533RE | Note |
|--------------|-------------|------|
| VCC | 3.3 V | |
| GND | GND | |
| SCL | PB6 (I2C1 SCL) | |
| SDA | PB7 (I2C1 SDA) | |

Notes:
- Use short, firm connections. Loose jumper wires are the most common cause of sensor problems (see Troubleshooting).
- Only the **Z axis** is used for analysis. Mount the sensor so its Z axis points along the direction you want to measure.
- Fix the sensor rigidly to the object. A loosely attached sensor produces noise that is not real vibration.

---

## 4. Quick Start

1. **Connect** the board to the laptop with the USB cable.
2. **Start an HTTP server** in the project folder (any port works; 5500 is used here):
   ```bash
   python -m http.server 5500
   ```
3. **Open the dashboard** in Chrome or Edge:
   ```
   http://localhost:5500/dashboard/index.html
   ```
4. Click **Choose port**, select the board's serial port, keep the baud rate at **115200**, and connect. The status shows _"Connected · waiting for calibration"_.
5. **Place the board and sensor on a flat, still surface.** Do not touch or move it.
6. **Press the black RESET button** on the STM32 board.
7. **Wait for calibration** (1000 samples, about 10 seconds). The status shows _"Calibrating · keep sensor still"_, then _"Calibration complete · choose a mode"_.
8. **Choose a mode** as needed (see Section 5 or 6).

> **Important:** connect to the port _before_ pressing RESET. Calibration messages are only sent at boot. If you connect after calibration has already finished, the dashboard stays in "waiting for calibration" and its buttons remain disabled. Press RESET again to fix it.

> **Tip:** if the hardware was recently moved or re-wired, always re-run calibration on a flat, still surface.

> **Tip:** calibrate on the surface where the device will actually be used. Low/high vibration classes depend on that surface (see Section 10).

---

## 5. Recording a Dataset

1. In the **Recording** panel, choose a **Dataset class**:
   - `Stationary`, `Low vibration`, `High vibration`: every row is labeled with that fixed class.
   - `Inference label`: each row is labeled with the AI's own prediction.
2. Click **Start recording**. The panel shows _"Recording · N rows"_.
3. Keep the machine or object in the state you are recording (e.g. only low vibration) for the whole session.
4. Click **Stop session**. The session appears under **Saved recording sessions**.
5. Click **Download CSV** to save it (`vibeflow-session-<id>.csv`).

Tips for good data:
- Record **one class per session**; do not change the vibration level mid-session.
- Record several sessions per class, and keep the sensor position identical across sessions.
- Let the vibration stabilize a few seconds before pressing Start.
- One row is produced about every 1.28 s.

Saved sessions live in the browser page only. **Downloading before you refresh or close the tab is essential**, otherwise the data is lost.

---

## 6. Running Inference

1. After calibration, click **Start inference**.
2. The dashboard shows:
   - **Current classification** and its confidence (%)
   - **Detection history** (time, class, confidence)
   - Counters for each class
3. Click **Stop session** to leave inference mode.

A new result appears about every 1.28 s. Results may change for a moment when the vibration level changes, because each result covers a 2.56 s window.

---

## 7. Using a Serial Terminal Instead of the Dashboard

You can skip the dashboard and use any serial terminal (PuTTY, Tera Term, Arduino Serial Monitor, `screen`, etc.).

- Settings: **115200 baud, 8N1**, no flow control.
- After reset the board prints its startup log and calibration status, then the prompt:
  ```
  Mode selection:
    r = record dataset
    i = inference
  Input [r/i]:
  ```
- Select the mode manually by typing one character:

| Key | Action |
|-----|--------|
| `r` | Start recording. Then choose the class: `0` stationary, `1` low, `2` high, `3` inference label |
| `i` | Start inference |
| `s` | Stop the current session and return to the mode prompt |

Only one program can use the serial port at a time. Close the terminal before using the dashboard, and vice versa.

---

## 8. Serial Output Format

Lines starting with `#` are status or info messages. Everything else is data.

**Status markers**

| Line | Meaning |
|------|---------|
| `# calibration_samples=1000;keep_sensor_flat_and_still` | Calibration started |
| `# calibration_done` | Calibration finished |
| `# mode_prompt` | Waiting for `r` or `i` |
| `# class_prompt` | Waiting for class `0`-`3` |
| `# record_start,session_id=...` | Recording started |
| `# record_stop,session_id=...` | Recording stopped |
| `# inference_ready` | Inference active |
| `# inference_stop` | Inference stopped |

**CSV header (recording)**
```
label,class_name,session_id,frame,start_ms,end_ms,span_ms,hop_ms,peak_bin,peak_hz,peak_mg,rms_mg,band_0_5_mg,band_5_15_mg,band_15_30_mg,band_30_50_mg,read_err,push_err,ring_ovf,wake_ovf
```

Each row is one 256-sample frame (100 Hz sampling, 50 % overlap). Frequency bands: 0-5, 5-15, 15-30, and 30-50 Hz. The last four columns are error counters and should normally be `0`.

Reading with Python:
```python
import pandas as pd
df = pd.read_csv("vibeflow-session-1.csv", comment="#")
```

**Inference result (JSON, one per line)**
```json
{"class":"low_vibration","confidence":0.93}
```

---

## 9. Troubleshooting

| Symptom | Likely cause | What to do |
|---------|--------------|------------|
| Dashboard says "Web Serial is unavailable" | Wrong browser | Use current Chrome or Edge |
| Port list is empty | Cable is charge-only, or driver missing | Use a data cable, try another USB port, install the ST-LINK VCP driver |
| "Could not open serial port" | Port used by another program | Close other terminals or dashboard tabs, then retry |
| Stuck at "Waiting for calibration", buttons disabled | Connected after boot | Press the RESET button while the dashboard is connected |
| Calibration does not finish or sensor not detected (`who_am_i` wrong or calibration failed) | Loose or wrong wiring | Re-seat the wiring: check VCC, GND, SCL (PB6), SDA (PB7). Unplug and re-plug the USB cable, press RESET. If it still fails, **clean and rebuild** the firmware and flash again |
| Serial output looks correct but is irregular, scrambled, or values look wrong | Stale or corrupted build, or bad flash | **Clean the project, rebuild, and re-flash.** This fixed the issue in past cases |
| Garbled characters | Wrong baud rate | Set 115200 on both the dashboard/terminal and (if changed) the firmware |
| Classification looks wrong or never settles on "stationary" | Calibration was done on a moving or tilted surface | Put the board on a flat, still surface and press RESET to re-calibrate |
| Same vibration gives a different class than before | The device is on a different surface, or was calibrated somewhere else | Move it back, or re-calibrate on the current surface. Heavier surfaces need stronger vibration, lighter ones weaker (see Section 10) |
| Results are unstable between classes | Sensor is loose, or vibration is on the boundary between two classes | Fix the sensor rigidly; average over several results |
| Dashboard buttons stay disabled after a session | Session state lost | Click Stop session, or press RESET |
| Recorded sessions disappeared | Page was refreshed or closed | Sessions are kept in the browser only. Download each CSV right after recording |
| Non-zero `read_err`, `push_err`, `ring_ovf`, or `wake_ovf` in CSV | Sensor read errors or processing overload | Check wiring first; if it persists, discard that session and re-record |
| Board seems frozen | Firmware stuck | Press RESET. If it repeats, clean, rebuild, and re-flash |

**Recovery order (fastest first)**
1. Press RESET and wait for calibration.
2. Re-seat the sensor wiring and re-plug the USB cable.
3. Close every other program using the serial port.
4. Clean, rebuild, and re-flash the firmware.

---

## 10. Model and Training Data

### What runs on the device

The device runs a small neural network (an **MLP**, a fully connected network) on the microcontroller. It takes **6 features** computed from each 256-sample frame:

`peak_hz`, `peak_mg`, `rms_mg`, `band_0_5_mg`, `band_5_15_mg`, `band_15_30_mg`

and outputs one of three classes with a confidence value. The model is trained on a laptop, converted to an INT8-quantized TFLite model, and deployed to the board through CubeMX. Because the input is a handful of features instead of raw signal, inference is light enough for a small microcontroller.

### The two datasets

Two datasets were collected with this device and used for training experiments:

| | Dataset 1 (`data_1`) | Dataset 2 (`data_2`) |
|---|---|---|
| Purpose | Controlled vibration levels | Real vehicle detection |
| How it was made | Vibration applied **by hand** to the device, with a consistent, deliberate level for each class | Real vehicle data |
| Classes | `diam` (stationary), `low`, `high` | `no_vehicle`, `two_wheel`, `four_wheel`, `unknown` |
| Size | 900 frames, 30 sessions (10 per class, 30 frames each) | 2,969 frames, but only 6 sessions (mostly `no_vehicle` and `two_wheel`) |
| Status | Clean and balanced. **This is the dataset used for the deployed model.** | Not yet good enough for training. More data is needed. |

Notes on dataset 1:
- In the low-vibration data, the device really is vibrating slightly throughout the session. In the high-vibration data, it is vibrating strongly throughout. Each session holds one class only.
- Class names in the datasets map to the firmware and dashboard as: `diam` = Stationary, `low` = Low vibration, `high` = High vibration.
- A Random Forest baseline on the same features, evaluated with a session-level split (18 training, 6 validation, 6 test sessions), reached about 99 % accuracy on the test sessions. This shows the six features separate the three classes well. The MLP is the model actually deployed.

Notes on dataset 2:
- Vehicle classes are very imbalanced (for example, only 134 frames of `four_wheel` versus 1,777 of `no_vehicle`), and there are too few sessions to keep some sessions aside for testing.
- The `unknown` class is not yet well defined.
- Because of this, the vehicle model was **not** deployed. Dataset 2 needs more recordings before it can be trained reliably.

### Training files

The `Training/` folder contains the notebooks, to be run in order:

| File | What it does |
|------|--------------|
| `Phase_1_EDA.ipynb` | Exploratory analysis and cleaning of the recorded CSV files; produces the combined clean dataset |
| `phase_2_random_forest_baseline.ipynb` | Random Forest baseline, session-level train/validation/test split |
| `phase_3_tinyml_mlp.ipynb` | Trains small MLPs (e.g. 6 → 16 → 8 → 3), compares architectures, quantizes to INT8, exports the scaler values used on the device |

`data_1/vibration_clean_dataset.csv` and `data_2/vehicle_dataset.csv` are the combined datasets. Point the `DATASET_PATH` setting in the notebooks at the one you want to train on. In the current copy of Phase 3, it is set to the vehicle dataset. For the deployed vibration model, use dataset 1 with the classes `diam`, `low`, `high`.

Important: **split by session, never by frame.** Frames from the same session are very similar, so mixing them between training and test sets gives falsely high accuracy.

### Effect of the mounting surface

`low_vibration` and `high_vibration` are **not absolute vibration strengths**. They describe how strongly the sensor itself vibrates, and that depends on the surface the device sits on. The device is calibrated at startup on that surface, so its readings are relative to where it was placed.

| Surface | What happens |
|---------|--------------|
| **Heavy or rigid** (concrete floor, heavy table, machine frame) | The surface absorbs much of the energy, so a **stronger** vibration is needed before the sensor reaches `low_vibration` or `high_vibration` |
| **Light or flexible** (thin plate, light table, small box) | The surface moves easily, so a **weaker** vibration is enough to reach the same class |

The same source can therefore produce different classes on different surfaces. This is normal behavior, not a fault.

Practical guidance:
- Always **calibrate on the same surface where the device will be used** (Section 4).
- If you move the device to a different surface, place it flat and still and press RESET to re-calibrate.
- The training data was recorded on one particular setup. Results are most reliable on a similar surface. For a very different surface, record new data there (Section 5) and re-train, or expect the class boundaries to shift.
- Keep the mounting the same between recording and inference: same surface, same position, same fixing method.

### Collecting more data to improve the model

The device itself is the data collection tool:

1. Use **Recording** mode (Section 5) and pick the class for each session.
2. Record **many separate sessions per class**, in different conditions (different days, positions, intensities). Ten sessions per class is a minimum; more is better.
3. Keep the sensor mounting identical between recording and real use.
4. Keep classes balanced (similar number of frames per class).
5. Check that `read_err`, `push_err`, `ring_ovf`, and `wake_ovf` are `0`.
6. Retrain using the Phase 1 to 3 notebooks, then import the new model through CubeMX and rebuild the firmware.

If the scaler values (mean and scale) change after retraining, update them in the firmware too. A mismatch between the training scaler and the device scaler makes predictions wrong even when the model itself is good.

---

## 11. Limitations

- Sampling is 100 Hz, so only vibration up to about **50 Hz** is analyzed.
- Only the **Z axis** is used.
- The deployed model was trained on hand-induced vibration (dataset 1). It classifies **vibration level**, not vibration source. It does not yet identify vehicle types; that needs more data (see Section 10).
- The model recognizes only the three trained classes. Unseen machines, mounting positions, or vibration types may be classified incorrectly. Re-record data and re-train for new setups.
- Calibration assumes the board is flat and still at boot.
- Class results depend on the mounting surface. The same vibration source can give `low_vibration` on a light surface and `high_vibration` on another, and heavier surfaces need stronger vibration to reach the same class (see Section 10).
- Use it in normal temperature conditions; the MPU6050 is not rated for harsh environments.

---

## 12. Quick Reference

```
1. Plug in USB
2. python -m http.server 5500
3. Open http://localhost:5500/dashboard/index.html
4. Choose port (115200)
5. Put the board on a flat, still surface
6. Press RESET, wait for calibration
7. Start recording or inference
```