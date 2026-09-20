# VibeFlow User Manual

**Vibration monitoring and classification on STM32H533RE + µT-Kernel 3.0**
Version: firmware 0.1.0 · model 0.1.0 · dashboard 0.1.0

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
10. [Limitations](#10-limitations)
11. [Quick Reference](#11-quick-reference)

---

## 1. What is VibeFlow?

VibeFlow measures vibration with an MPU6050 accelerometer (GY-86 module), processes it on an STM32H533RE, and classifies it into three states:

| Label | Class | Meaning |
|-------|-------|---------|
| 0 | `stationary` | No vibration |
| 1 | `low_vibration` | Light vibration |
| 2 | `high_vibration` | Strong vibration |

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

## 10. Limitations

- Sampling is 100 Hz, so only vibration up to about **50 Hz** is analyzed.
- Only the **Z axis** is used.
- The model recognizes only the three trained classes. Unseen machines, mounting positions, or vibration types may be classified incorrectly. Re-record data and re-train for new setups.
- Calibration assumes the board is flat and still at boot.
- Use it in normal temperature conditions; the MPU6050 is not rated for harsh environments.

---

## 11. Quick Reference

```
1. Plug in USB
2. python -m http.server 5500
3. Open http://localhost:5500/dashboard/index.html
4. Choose port (115200)
5. Put the board on a flat, still surface
6. Press RESET, wait for calibration
7. Start recording or inference
```
