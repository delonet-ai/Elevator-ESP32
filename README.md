# 🚀 Elevator ESP32 — Mini Lift on ESP32  
**Wireless remote, OLED animations, full calibration, smooth motor motion**

> **Repository:** https://github.com/delonet-ai/Elevator-ESP32  
> This project implements a fully functional toy elevator system built on **two ESP32 microcontrollers**:  
> - `LiftController` — motor control, logic, calibration, sensors  
> - `RemoteControl` — wireless remote with OLED UI and button LEDs  

The system is **autonomous**, does **not** require Wi-Fi, and uses **ESP-NOW protocol** with a full **state-machine architecture**.

# 📁 Project Structure

```
Elevator-ESP32/
├── LiftController/          # Main elevator logic
│   ├── src/
│   │   ├── motor_controller.cpp
│   │   ├── state_machine.cpp
│   │   ├── espnow_handler.cpp
│   │   └── ...
│   ├── include/
│   ├── platformio.ini
│   └── README_LIFT.md
│
├── RemoteControl/           # Wireless remote with OLED UI
│   ├── src/
│   │   ├── remote_ui.cpp
│   │   ├── espnow_remote.cpp
│   │   ├── button_leds.cpp
│   │   └── ...
│   ├── include/
│   ├── platformio.ini
│   └── README_REMOTE.md
│
├── docs/
│   ├── wiring/
│   ├── pics/
│   ├── protocol.md
│   └── state_machine.md
│
└── README.md
```

# ✨ Features Overview

### 🎛 Three Floors  
Precise floor tracking, auto slowdown before stopping.

### 🛰 Wireless Remote (ESP-NOW)  
Low-latency ESP-NOW link, no Wi-Fi needed.

### 📟 Animated OLED Interface  
- Big floor number  
- Animated arrow (up/down/idle)  
- Status text  
- Calibration prompts  

### 🔦 Button Backlighting  
- UP/DOWN always on  
- Current floor blinks  
- Selected target lights up  
- Calibration mode highlights only relevant buttons  

### 🧭 Smooth Motor Control  
- Acceleration & deceleration  
- Cruise speed  
- Braking distance calculation  
- Smooth start/stop  

### 🎚 Speed Control  
Potentiometer sets the motor’s speed limit.

### 🧰 Full Calibration System  
- Top homing to limit switch  
- Downward travel measurement  
- Bottom point confirmation  
- Automatic floor division  

### ⚡ Fast Down Calibration  
Downward speed multiplier during calibration phase.

### 🛠 Manual Motion Mode  
UP/DOWN buttons allow manual motion control.

# 🔧 Hardware Overview (BOM)

## 🧠 Base Unit (LiftController)

| Component | Purpose |
|----------|---------|
| ESP32 DevKit | Main controller |
| NEMA stepper motor | Move the cabin |
| A4988 / DRV8825 / TMC2208 | Stepper driver |
| Upper limit switch | Detect the top of travel |
| Potentiometer | Speed limiter |
| Button (GPIO 33) | Recalibration |
| 3S Li-ion + BMS | Main power |
| DC-DC 12→5 V | ESP32 power |

### Base Unit Pins

| Function | Pin |
|---------|-----|
| STEP | 18 |
| DIR | 19 |
| ENABLE | 21 |
| Limit switch | A1 |
| Potentiometer | A2 |
| Calib button | 33 |

## 🎮 Remote Unit (RemoteControl)

| Component | Purpose |
|----------|---------|
| ESP32 DevKit | UI + communication |
| OLED SSD1306 128×32 | Display |
| 5 buttons | Floors + UP/DOWN |
| 5 LEDs | Button lighting |
| 1S Li-ion | Power |
| TP4056 / BMS | Charging |

### Remote Control Buttons

| Button | Pin |
|--------|-----|
| Down | 13 |
| Up | 14 |
| Floor 1 | 27 |
| Floor 2 | 26 |
| Floor 3 | 25 |

### Remote LEDs

| LED | Pin |
|------|----|
| LED Down | 19 |
| LED Up | 18 |
| LED F1 | 5 |
| LED F2 | 4 |
| LED F3 | 33 |

### OLED Pins

| Signal | Pin |
|--------|-----|
| SDA | 21 |
| SCL | 22 |

# 📡 ESP-NOW Communication

### Commands (Remote → Lift)

```
CMD_CALL_FLOOR
CMD_STOP
CMD_CALIB
CMD_CALIB_DOWN_START
CMD_CALIB_DOWN_SAVE
CMD_MANUAL_UP
CMD_MANUAL_DOWN
CMD_MANUAL_STOP
```

### Status Message (Lift → Remote)  
Sent **every 200 ms**

```
state
currentFloor
targetFloor
direction
speedPercent
error
needCalib
uptime
```

# 🔁 LiftController State Machine

Main states:

```
STATE_BOOT
STATE_NEED_CALIB
STATE_CALIB_HOMING_UP
STATE_CALIB_MOVING_DOWN
STATE_IDLE
STATE_MOVING
STATE_MANUAL_MOVE
STATE_ERROR
```

State tick interval: **20 ms**

# 🧮 Calibration Logic

### Initial Calibration

1. System starts in `STATE_NEED_CALIB`
2. Press **UP** → move to upper limit  
3. Limit triggered → save top position  
4. Press **DOWN** → move downward  
5. Press **Floor 1** → save bottom point  
6. Elevator ready → `STATE_IDLE`

### Recalibration  
- Hold **GPIO33** ≥ 3 seconds → reset EEPROM and restart calibration

# 🖥 OLED UI Structure

```
+----------------+------------------+-----------------+
|  Status text   |   Big floor #    |  Arrow anim     |
|                |     (size=3)     |     ↑ ↓ ↔       |
+----------------+------------------+-----------------+
```

Arrow animation:  
- Scrolling segments upward → UP  
- Scrolling segments downward → DOWN  
- Thick horizontal bar → idle  
- Refresh every ~150 ms

# ⚙ Motor Controller

- Soft acceleration  
- Soft braking  
- Braking distance calculation (`v² / 2a`)  
- Auto-stop at destination  
- Separate manual mode logic  

### Fast downward calibration example:

```cpp
if (calibDownFastFlag && manualDir < 0) {
    baseSpeed *= X;   // Faster descending speed
}
```

# 📐 Wiring Diagram (ASCII Placeholder)

```
<INSERT YOUR ASCII DIAGRAM HERE>
```

# 🧪 Project Status

| Feature | Status |
|---------|--------|
| Smooth motor movement | ✔ |
| Wireless remote | ✔ |
| ESP-NOW stable link | ✔ |
| Reliable calibration | ✔ |
| OLED UI | ✔ |
| Button lighting | ✔ |
| Fast calibration down | ✔ |
