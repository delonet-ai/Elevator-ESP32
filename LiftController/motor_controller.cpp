#include "motor_controller.h"
#include <math.h>

// Пины (как мы договорились)
static const int STEP_PIN = 18;
static const int DIR_PIN  = 19;
static const int EN_PIN   = 21;

// Профиль движения
static float maxSpeed   = 2500.0f;  // шагов/сек (максимальная крейсерская)
static float accel      = 1800.0f;   // шагов/сек^2 (ускорение/торможение)
static float manualSpeed = 400.0f;  // шагов/сек в ручном режиме

// Текущее состояние движения
static long  currentPos     = 0;      // текущая позиция в шагах
static long  targetPos      = 0;      // целевая позиция в шагах
static bool  moveActive     = false;  // едем к targetPos
static bool  manualMode     = false;  // ручной режим (MAN_UP / MAN_DOWN)
static int   manualDir      = 0;      // +1 вверх, -1 вниз
static bool calibDownFastFlag = false; //быстрее при калибровке вниз
static float currentSpeed   = 0.0f;   // текущая скорость (шагов/сек, знак = направление)
static unsigned long lastStepMicros   = 0;  // момент последнего шага
static unsigned long lastServiceMicros = 0; // момент последнего пересчёта скорости

// Минимальная скорость, ниже которой не шагаем (чтобы избежать дёрганий)
static const float MIN_SPEED = 50.0f;         // шагов/сек
// Минимальная длительность импульса STEP
static const int   STEP_PULSE_US = 2;

// ----------------------------------------------------------

void motorInit() {
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);

  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, LOW);
  // EN активен по LOW → сразу включаем драйвер
  digitalWrite(EN_PIN, LOW);

  currentPos = 0;
  targetPos  = 0;
  moveActive = false;
  manualMode = false;
  manualDir  = 0;
  currentSpeed = 0.0f;
  lastStepMicros = micros();
  lastServiceMicros = micros();

  Serial.println("[MOTOR] Init: STEP=18, DIR=19, EN=21");
}
void motorCalibDownFast() {
  manualMode = true;
  moveActive = false;
  manualDir  = -1;
  calibDownFastFlag = true; 
  // Удваиваем скорость
  currentSpeed = 0;
  Serial.println("[MOTOR] Calib DOWN FAST (x2)");

  // временно увеличиваем manualSpeed
  // но только внутри motorService
}

static void setDirFromSpeed(float speed) {
  // Положительная скорость → DIR HIGH (допустим, вверх)
  // Если хочешь инвертировать направление — просто поменяй HIGH/LOW местами
  if (speed >= 0) {
    digitalWrite(DIR_PIN, HIGH);
  } else {
    digitalWrite(DIR_PIN, LOW);
  }
}

void motorSetMaxSpeed(float speed_steps_per_sec) {
  if (speed_steps_per_sec < MIN_SPEED) speed_steps_per_sec = MIN_SPEED;
  maxSpeed = speed_steps_per_sec;
  // Serial.print("[MOTOR] maxSpeed="); Serial.println(maxSpeed);
}

void motorSetAccel(float accel_steps_per_sec2) {
  if (accel_steps_per_sec2 < 10.0f) accel_steps_per_sec2 = 10.0f;
  accel = accel_steps_per_sec2;
  // Serial.print("[MOTOR] accel="); Serial.println(accel);
}

void motorUpdateSpeedFromPot(int potRaw) {
  // Мапим 0..4095 → 200..2000 шаг/сек
  float s = 200.0f + (1800.0f * ((float)potRaw / 4095.0f));
  motorSetMaxSpeed(s);
}

// ----------------------------------------------------------
// Внешние команды движения

void motorMoveTo(long targetPosition) {
  targetPos = targetPosition;
  moveActive = true;
  manualMode = false;
  manualDir  = 0;
  // направление задаём по знаку distanceToGo в motorService()
  Serial.print("[MOTOR] MoveTo "); Serial.println(targetPos);
}

void motorStop() {
  moveActive = false;
  manualMode = false;
  manualDir  = 0;
  currentSpeed = 0.0f;
  calibDownFastFlag = false;  // <----------- СБРОС
  Serial.println("[MOTOR] Stop");
}

void motorManualUp() {
  manualMode = true;
  moveActive = false;
  manualDir  = +1;
  currentSpeed = 0.0f; // начнём разгоняться вверх
  Serial.println("[MOTOR] Manual UP");
}

void motorManualDown() {
  manualMode = true;
  moveActive = false;
  manualDir  = -1;
  currentSpeed = 0.0f; // начнём разгоняться вниз
  Serial.println("[MOTOR] Manual DOWN");
}

void motorManualStop() {
  manualMode = false;
  manualDir  = 0;
  currentSpeed = 0.0f;
  Serial.println("[MOTOR] Manual STOP");
}

// ----------------------------------------------------------
// Позиция

long motorGetCurrentPosition() {
  return currentPos;
}

void motorSetCurrentPosition(long pos) {
  currentPos = pos;
  // При установке позиции мы также ставим targetPos = currentPos,
  // чтобы не было "ложного" движения
  targetPos = pos;
  Serial.print("[MOTOR] Set position="); Serial.println(currentPos);
}

// ----------------------------------------------------------
// Внутренние вспомогательные функции

// Один шаг в текущем направлении currentSpeed
static void doStep() {
  // Выбираем направление по знаку скорости
  setDirFromSpeed(currentSpeed);

  digitalWrite(STEP_PIN, HIGH);
  delayMicroseconds(STEP_PULSE_US);
  digitalWrite(STEP_PIN, LOW);

  // Обновляем логическую позицию
  if (currentSpeed > 0) {
    currentPos += 1;
  } else if (currentSpeed < 0) {
    currentPos -= 1;
  }
}

// ----------------------------------------------------------
// Главная функция сервиса, вызывается в loop() очень часто

void motorService() {
  unsigned long nowMicros = micros();
  float dt = (nowMicros - lastServiceMicros) / 1000000.0f; // dt в секундах
  if (dt <= 0) dt = 0.000001f;
  lastServiceMicros = nowMicros;

  // Определяем, чего мы хотим: режим MoveTo или Manual
  float targetSpeed = 0.0f; // желаемая скорость (шаг/сек)

if (manualMode) {
    float baseSpeed = manualSpeed;

    // если включён быстрый режим калибровки — умножаем
    if (calibDownFastFlag && manualDir < 0) {
        baseSpeed *= 3.0f;   // ← множитель скорости
    }

    if (manualDir > 0) {
        targetSpeed = baseSpeed;
    } else if (manualDir < 0) {
        targetSpeed = -baseSpeed;
    } else {
        targetSpeed = 0.0f;
    }
}
 else if (moveActive) {
    long distanceToGo = targetPos - currentPos; // сколько шагов осталось
    if (distanceToGo == 0) {
      // Уже на месте
      moveActive = false;
      currentSpeed = 0.0f;
      return;
    }

    int dir = (distanceToGo > 0) ? +1 : -1;
    float dist = fabs((float)distanceToGo);

    // Тормозной путь при текущей скорости: v^2 / (2a)
    float speedAbs = fabs(currentSpeed);
    float brakeDist = (speedAbs * speedAbs) / (2.0f * accel);

    float maxAllowedSpeed = maxSpeed;

    // Если пора тормозить, ограничиваем максимальную скорость
    if (brakeDist > dist) {
      maxAllowedSpeed = sqrtf(2.0f * accel * dist);
      if (maxAllowedSpeed < MIN_SPEED) {
        maxAllowedSpeed = MIN_SPEED;
      }
    }

    targetSpeed = dir * maxAllowedSpeed;
  } else {
    // Нет ни движения, ни ручного режима
    targetSpeed = 0.0f;
  }

  // Плавно изменяем currentSpeed → targetSpeed с учётом ускорения
  float maxDeltaV = accel * dt; // максимальное изменение скорости за dt

  if (currentSpeed < targetSpeed) {
    currentSpeed += maxDeltaV;
    if (currentSpeed > targetSpeed) currentSpeed = targetSpeed;
  } else if (currentSpeed > targetSpeed) {
    currentSpeed -= maxDeltaV;
    if (currentSpeed < targetSpeed) currentSpeed = targetSpeed;
  }

  // Если скорость почти нулевая — не шагаем
  if (fabs(currentSpeed) < MIN_SPEED) {
    return;
  }

  // Частота шагов → интервал между шагами
  float stepInterval = 1000000.0f / fabs(currentSpeed); // микросек на один шаг

  if ((nowMicros - lastStepMicros) >= (unsigned long)stepInterval) {
    lastStepMicros = nowMicros;
    doStep();
  }
}
