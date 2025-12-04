Версия на русском ниже.
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


Схема подключения базы


                      ┌──────────────────────────────┐
                      │          ESP32 BASE          │
                      │                              │
                      │   3V3 ──────────────┐        │
                      │   GND ───────┐      │        │
                      │              │      │        │
                      │        GPIO33 ◄────┘   Кнопка калибровки (HOLD 3s)
                      │        GPIOA1 ◄──────── Концевик верхний (NO → GND)
                      │        GPIOA2 ◄──────── Потенциометр (средний вывод)
                      │                              │
                      │        GPIO18 ─────► STEP    │
                      │        GPIO19 ─────► DIR     │
                      │        GPIO21 ─────► ENABLE  │
                      └──────────┬───────────────────┘
                                 │
                     ┌───────────┴──────────────────────────┐
                     │        Драйвер A4988 / DRV8825        │
                     │                                        │
                     │   VMOT ◄────────────── 12V (3S Li-ion BMS OUT)
                     │   GND  ◄────────────── GND (общий с ESP32)
                     │   STEP ◄────────────── GPIO18
                     │   DIR  ◄────────────── GPIO19
                     │   EN   ◄────────────── GPIO21
                     │   1A/1B/2A/2B ──────── Шаговый мотор
                     │   VDD ◄────────────── 3.3–5V из DC-DC (логика)
                     └────────────────────────────────────────┘


 Потенциометр:
 
 ┌────────────┐
 │  3.3V  ◄───┤
 │  OUT  ───► GPIO A2
 │  GND  ◄───┤
 └────────────┘

 Концевик (верхний):
 
 ┌────────────┐
 │  COM  ◄───► GND
 │  NO   ───► GPIO A1
 └────────────┘

 Кнопка RESET-калибровки (на базе):
 
 ┌────────────┐
 │ один вывод → GPIO33
 │ другой     → GND
 └────────────┘

 Питание:
 
 ┌────────────────────────┐
 │  3S Li-ion (11.1–12.6V)│
 │        через BMS       │
 └──────────┬─────────────┘
            │ 12–12.6V → VMOT драйвера
            ├──────────────► DC-DC 5V → ESP32 5V
            └──────────────► общий GND


Пульт 

                      ┌──────────────────────────────┐
                      │         ESP32 REMOTE         │
                      │                              │
                      │   3V3 ───────┐               │
                      │   GND ───────┼─────────┐     │
                      │              │         │     │
                      │           SDA GPIO21 ──┼──► OLED SDA
                      │           SCL GPIO22 ──┼──► OLED SCL
                      │                              │
                      │   GPIO13 ◄────── Down Button │
                      │   GPIO14 ◄────── Up Button   │
                      │   GPIO27 ◄────── Floor1 Btn  │
                      │   GPIO26 ◄────── Floor2 Btn  │
                      │   GPIO25 ◄────── Floor3 Btn  │
                      │                              │
                      │   GPIO19 ──────► LED Down    │
                      │   GPIO18 ──────► LED Up      │
                      │   GPIO5  ──────► LED F1      │
                      │   GPIO4  ──────► LED F2      │
                      │   GPIO33 ──────► LED F3      │
                      └────────────┬──────────────────┘
                                   │
                              ┌────┴─────────┐
                              │   OLED 128x32│
                              │  VCC → 3.3V  │
                              │  GND → GND   │
                              │  SDA → GPIO21│
                              │  SCL → GPIO22│
                              └──────────────┘


 Кнопки:
 
 ┌────────────┐
 │ один вывод → GPIOXX (Input PullUp)
 │ другой     → GND
 └────────────┘

 Светодиоды (с внутренним резистором):
 
 ┌───────────────┐
 │  LED + → GPIOXX
 │  LED – → GND
 └───────────────┘

 Питание пульта:
 
 ┌──────────────┐
 │ 1S Li-ion     │
 │ TP4056        │ → 3.3V стабилизатор → ESP32 3.3V
 └──────────────┘
 (либо LDO AMS1117-3.3)



**Elevator ESP32 —  мини-лифт ESP32**
С беспроводным пультом, OLED-анимацией, калибровкой и плавным движением

Этот проект — полноценный обучающий мини-лифт (для игрушек), построенный на базе двух ESP32:
Base ESP32 — управляет мотором, калибровкой, движением, логикой и датчиками.
Remote ESP32 — беспроводной пульт с OLED-экраном, подсветкой кнопок и ESP-NOW связью.

Проект полностью автономный, работает без Wi-Fi, использует собственный протокол и полноценную state-machine архитектуру.

**✨ Возможности**
🎛 3 этажа
Лифт точно знает текущий этаж, умеет ездить к любому, тормозит перед остановкой.
🛰 Беспроводной пульт (ESP-NOW)

**📟 OLED-интерфейс с анимацией**
Пульт показывает:
текущий этаж (крупный, в центре экрана)
направление движения (анимированная стрелка как в настоящих лифтах)
текстовый статус слева
ошибки / калибровку

**🔦 Подсветка всех кнопок**
Up/Down — всегда включены
Кнопка текущего этажа — мигает
Кнопка выбранного этажа — горит
В калибровке — подсвечиваются только нужные кнопки

**🧭 Плавное движение**
Мотор работает с:
плавным разгоном
крейсерской скоростью
торможением перед остановкой
ограничением скорости по ходу

**🎚 Регулировка скорости**
Потенциометр задаёт максимальную скорость движения.

**🧰 Полная система калибровки**
На первом запуске или после сброса:
Нажимаем UP → лифт едет вверх до концевика и записывает верхнюю позицию.
Нажимаем DOWN → лифт едет вниз.
Нажимаем кнопку 1 этажа → фиксируется нижняя точка.
Лифт автоматически считает длину троса и делит её на 3 этажа.

**🔁 Повторная калибровка**
На базе есть физическая кнопка на GPIO 33:
удержание 3 секунды → полный сброс калибровки
лифт автоматически входит в режим первичной настройки
⚡ Быстрая калибровка вниз
Во время калибровки скорость опускания ниже может быть увеличена (множитель).

**🛠 Режим ручного управления**
Кнопки UP/DOWN на пульте позволяют вручную перемещать лифт.

**🔧 Железо (BOM)**
**🧠 База (лифт)**
ESP32 DevKit	контроллер лифта
Шаговый мотор NEMA + драйвер (A4988 / DRV8825 / TMC2208)	движение кабины
Концевик верхний	определение верхнего этажа
Потенциометр	регулировка скорости
Кнопка на GPIO33	принудительная калибровка
Питание 3S Li-ion + BMS	силовая часть
DC-DC 12→5	питание ESP32

**Пины базы**
STEP	18
DIR	19
ENABLE	21
Верхний концевик	A1
Потенциометр	A2
Кнопка калибровки	33

**🎮 Пульт (remote ESP32)**
ESP32 DevKit	коммуникация + UI
OLED SSD1306 128×32	интерфейс
5 кнопок	этажи + вверх + вниз
5 светодиодов	индикация
Питание 1S Li-ion	автономность
TP4056/BMS	зарядка и защита
  Пины пульта
Down	13
Up	14
Floor 1	27
Floor 2	26
Floor 3	25
  Светодиоды
LED Down	19
LED Up	18
LED F1	5
LED F2	4
LED F3	33
  OLED
SDA	21
SCL	22

📡 Протокол ESP-NOW
Команды пульта → база
CMD_CALL_FLOOR
CMD_STOP
CMD_CALIB
CMD_CALIB_DOWN_START
CMD_CALIB_DOWN_SAVE
CMD_MANUAL_UP
CMD_MANUAL_DOWN
CMD_MANUAL_STOP

Статус база → пульт
state (текущее состояние автомата)
currentFloor
targetFloor
direction
speedPercent
error
needCalib
uptime

Статус отправляется каждые 200 мс.

**🔁 State Machine (база)**
Основные состояния:
STATE_BOOT
STATE_NEED_CALIB
STATE_CALIB_HOMING_UP
STATE_CALIB_MOVING_DOWN
STATE_IDLE
STATE_MOVING
STATE_MANUAL_MOVE
STATE_ERROR
Тик автомата каждые 20 мс.

**🧮 Калибровка**
Первый запуск
Лифт стоит в STATE_NEED_CALIB
Нажимаем UP → выезд вверх (STATE_CALIB_HOMING_UP)
Концевик → фиксируем верх
Нажимаем DOWN → спуск вниз (STATE_CALIB_MOVING_DOWN)
Нажимаем Floor 1 → фиксируем низ
Лифт готов (STATE_IDLE)

Повторная калибровка
GPIO33 (удержание ≥3s) → полный сброс EEPROM.
🖥 OLED-интерфейс пульта

**Экран разделён на три зоны:**

+----------------+------------------+-----------------+
|   Статус       |  Крупный этаж    |  Анимация       |
|   (текст)      |      (size=3)    |  стрелки ↑↓↔    |
+----------------+------------------+-----------------+

Анимация стрелки
Вверх → внутри стрелки “бегут сегменты” вверх
Вниз → сегменты бегут вниз
Стоим → толстая горизонтальная линия
Обновление по millis() каждые ~150 мс.

**⚙ Моторный контроллер**
Плавное ускорение и торможение
Рассчёт тормозного пути (v² / (2a))
Режим MoveTo с автоторможением

Режим Manual
Отдельная логика для быстрой калибровки вниз:
if (calibDownFastFlag && manualDir < 0) {
    baseSpeed *= X;   // множитель скорости
}

**🧪 Статус проекта**

✔ Mотор двигает кабину плавно
✔ Пульт полностью работает
✔ ESP-NOW стабильный
✔ Калибровка надёжная
✔ OLED-UI красивый и понятный ребёнку
✔ Подсветка кнопок реализована
✔ Быстрая калибровка вниз работает


