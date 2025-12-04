#include "calibration_manager.h"
#include "motor_controller.h"
#include "floor_manager.h"

// Запас от верхнего концевика до 3-го этажа (в шагах)
static const long TOP_MARGIN_STEPS = 200;   // подберёшь опытно
static const long MIN_TRAVEL_STEPS = 200;   // минимальный допустимый ход

// Наш внутренний флаг валидности калибровки
static bool g_calibValid = false;

void calibInit() {
  Serial.println("[CALIB] Init");

  // Синхронизируем флаг с менеджером этажей.
  // floorHasValidCalibration() уже знает, есть ли сохранённая калибровка.
  g_calibValid = floorHasValidCalibration();
}

bool calibHasValidData() {
  return g_calibValid;
}

void calibForceReset() {
  Serial.println("[CALIB] Force reset calibration");

  // Сброс хода лифта.
  // 0 шагов однозначно означает "нет калибровки" для floor_manager.
  floorSetFullTravelSteps(0);

  g_calibValid = false;
}

// Шаг 1: старт хоминга вверх до концевика
void calibStartHomingUp() {
  Serial.println("[CALIB] Homing UP: manual up");
  // Едем вверх в ручном режиме.
  // Направление вверх задано в motorManualUp() (через знак скорости).
  motorManualUp();
}

// Вызывается, когда в STATE_CALIB_HOMING_UP сработал верхний концевик
void calibOnTopReached() {
  // Останавливаемся на концевике
  motorStop();

  // Считаем точку концевика как 0 для калибровки
  motorSetCurrentPosition(0);
  Serial.println("[CALIB] Top reached, position set to 0 (TopSwitch)");
}

// Шаг 2: команда на движение вниз в калибровке
void calibStartMovingDown() {
  Serial.println("[CALIB] Moving DOWN for calibration (manual down)");
  // Просто едем вниз. Позиция будет уходить в минус.
  motorCalibDownFast();
}

// Шаг 3: остановка внизу и сохранение калибровки
void calibSaveBottom() {
  // Остановить движение
  motorStop();

  // Текущая позиция (будет отрицательной, т.к. от 0 (верх) поехали вниз)
  long bottomPos = motorGetCurrentPosition();

  Serial.print("[CALIB] Bottom raw position = ");
  Serial.println(bottomPos);

  long distanceBottomToTopSwitch = labs(bottomPos);

  Serial.print("[CALIB] Distance Bottom -> TopSwitch = ");
  Serial.println(distanceBottomToTopSwitch);

  // Учитываем запас сверху от концевика до 3-го этажа
  long full = distanceBottomToTopSwitch - TOP_MARGIN_STEPS;

  if (full < MIN_TRAVEL_STEPS) {
    Serial.print("[CALIB] WARNING: fullTravelSteps too small (");
    Serial.print(full);
    Serial.print("), forcing to MIN_TRAVEL_STEPS=");
    Serial.println(MIN_TRAVEL_STEPS);
    full = MIN_TRAVEL_STEPS;
  }

  // Передаём реальный ход в менеджер этажей
  floorSetFullTravelSteps(full);

  // Переносим систему координат: низ = 0
  motorSetCurrentPosition(0);

  // Калибровка теперь валидна
  g_calibValid = true;

  Serial.print("[CALIB] Calibration done. fullTravelSteps=");
  Serial.println(full);
  Serial.println("[CALIB] Floor1=0, Floor2=full/2, Floor3=full (below top switch)");
}

void calibUpdate() {
  // Пока пустой, на будущее (можно оставить no-op)
}
