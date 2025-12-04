#include "state_machine.h"
#include "motor_controller.h"
#include "io_manager.h"
#include "floor_manager.h"
#include "calibration_manager.h"

// Текущее состояние автомата
static LiftState state = STATE_BOOT;
static uint8_t currentFloor = 0;
static uint8_t targetFloor  = 0;
static long    targetPosition = 0;
static int     errorCode = 0;

// Простые константы для логики движения
static const long POSITION_TOLERANCE          = 10;     // в шагах
static const unsigned long MOTION_TIMEOUT_MS  = 20000;  // таймаут движения

static unsigned long motionStartTime = 0;

void smInit() {
  // Выбираем начальное состояние в зависимости от калибровки
  if (calibHasValidData()) {
    state = STATE_IDLE;
    currentFloor = floorGetNearestFloor(motorGetCurrentPosition());
    Serial.println("[SM] Calibration OK, starting in IDLE");
  } else {
    state = STATE_NEED_CALIB;
    Serial.println("[SM] No calibration, NEED_CALIB");
  }
}

void smTick() {
  // Обновляем скорость по потенциометру
  int potRaw = ioReadPotSpeed();
  motorUpdateSpeedFromPot(potRaw);

  // Верхний концевик — базовая реакция
  bool topSwitch = ioReadTopSwitch();

  switch (state) {
    case STATE_BOOT:
      // сюда не должны попадать после smInit, но на всякий случай
      break;

    case STATE_NEED_CALIB:
      // Ждём smCommandStartCalib() (с пульта или через Serial)
      break;

    case STATE_CALIB_HOMING_UP:
      if (topSwitch) {
        Serial.println("[SM] CALIB: reached top switch");
        calibOnTopReached();  // сообщаем модулю калибровки
        state = STATE_CALIB_MOVING_DOWN;
      }
      break;

    case STATE_CALIB_MOVING_DOWN:
      // Здесь уже идёт движение вниз; ждём smCommandCalibDownSave()
      break;

    case STATE_IDLE:
      // Можно следить за кнопками STOP/Calib и т.п. через io_manager
      break;

    case STATE_MOVING: {
      long currentPos = motorGetCurrentPosition();
      long diff       = targetPosition - currentPos;
      long distAbs    = labs(diff);

      // Проверка окончания движения (упрощённо)
      if (distAbs <= POSITION_TOLERANCE) {
        motorStop();
        currentFloor = targetFloor;
        targetFloor  = 0;
        state = STATE_IDLE;
        Serial.print("[SM] Reached target floor: ");
        Serial.println(currentFloor);
      }

      // Таймаут
      if (millis() - motionStartTime > MOTION_TIMEOUT_MS) {
        Serial.println("[SM] Motion timeout! ERROR");
        motorStop();
        state = STATE_ERROR;
        errorCode = 1;
      }

      // Неожиданный верхний концевик при движении вверх
      if (topSwitch && diff > 0) {  // ехали вверх
        Serial.println("[SM] Unexpected top switch! ERROR");
        motorStop();
        state = STATE_ERROR;
        errorCode = 2;
      }
      break;
    }

    case STATE_MANUAL_MOVE:
      // Логика ручного движения реализуется через команды MANUAL_* и мотор
      break;

    case STATE_ERROR:
      // Ждём smCommandClearError() или smForceNeedCalib()
      break;
  }
}

// ---------------- команды извне ----------------

void smCommandMoveToFloor(uint8_t floor) {
  if (state == STATE_NEED_CALIB || state == STATE_CALIB_HOMING_UP || state == STATE_CALIB_MOVING_DOWN) {
    Serial.println("[SM] Move command ignored: NEED_CALIB/CALIB");
    return;
  }
  if (state == STATE_ERROR) {
    Serial.println("[SM] Move command ignored: ERROR state");
    return;
  }
  if (floor < 1 || floor > 3) {
    Serial.println("[SM] Invalid floor");
    return;
  }
  if (state != STATE_IDLE && state != STATE_MOVING) {
    Serial.println("[SM] Move command ignored: not in IDLE/MOVING");
    return;
  }

  long currentPos = motorGetCurrentPosition();
  long dest       = floorGetPositionForFloor(floor);

  if (labs(dest - currentPos) <= POSITION_TOLERANCE) {
    Serial.print("[SM] Already at floor ");
    Serial.println(floor);
    currentFloor = floor;
    targetFloor  = 0;
    state = STATE_IDLE;
    return;
  }

  targetFloor     = floor;
  targetPosition  = dest;
  state           = STATE_MOVING;
  motionStartTime = millis();
  motorMoveTo(targetPosition);

  Serial.print("[SM] Moving to floor ");
  Serial.print(floor);
  Serial.print(" (target pos ");
  Serial.print(targetPosition);
  Serial.println(")");
}

void smCommandStop() {
  motorStop();
  if (state == STATE_MOVING || state == STATE_MANUAL_MOVE) {
    Serial.println("[SM] STOP: motor stopped, go to IDLE");
    state = STATE_IDLE;
    targetFloor = 0;
  } else {
    Serial.println("[SM] STOP: no movement");
  }
}

void smCommandStartCalib() {
  if (state == STATE_MOVING || state == STATE_MANUAL_MOVE) {
    Serial.println("[SM] Cannot start calib while moving");
    return;
  }
  Serial.println("[SM] Start calibration: homing up");
  state = STATE_CALIB_HOMING_UP;
  calibStartHomingUp();
}

void smCommandCalibDownStart() {
  if (state != STATE_CALIB_MOVING_DOWN) {
    Serial.println("[SM] CALIB_DOWN_START ignored: not in CALIB_MOVING_DOWN");
    return;
  }
  Serial.println("[SM] Start moving down for calibration");
  calibStartMovingDown();
}

void smCommandCalibDownSave() {
  if (state != STATE_CALIB_MOVING_DOWN) {
    Serial.println("[SM] CALIB_DOWN_SAVE ignored: not in CALIB_MOVING_DOWN");
    return;
  }
  Serial.println("[SM] Save bottom position");
  calibSaveBottom();
  currentFloor = 1;
  targetFloor  = 0;
  state        = STATE_IDLE;
}

void smCommandManualUpStart() {
  if (state == STATE_ERROR || state == STATE_CALIB_HOMING_UP || state == STATE_CALIB_MOVING_DOWN) {
    Serial.println("[SM] MAN_UP ignored in current state");
    return;
  }
  Serial.println("[SM] Manual move UP");
  state = STATE_MANUAL_MOVE;
  motorManualUp();
}

void smCommandManualDownStart() {
  if (state == STATE_ERROR || state == STATE_CALIB_HOMING_UP || state == STATE_CALIB_MOVING_DOWN) {
    Serial.println("[SM] MAN_DOWN ignored in current state");
    return;
  }
  Serial.println("[SM] Manual move DOWN");
  state = STATE_MANUAL_MOVE;
  motorManualDown();
}

void smCommandManualStop() {
  if (state == STATE_MANUAL_MOVE) {
    Serial.println("[SM] Manual move STOP");
    motorStop();
    state = STATE_IDLE;
  }
}

void smCommandClearError() {
  if (state != STATE_ERROR) {
    Serial.println("[SM] CLEAR: not in ERROR");
    return;
  }
  Serial.println("[SM] CLEAR: error cleared");
  errorCode = 0;

  if (calibHasValidData()) {
    state = STATE_IDLE;
  } else {
    state = STATE_NEED_CALIB;
  }
}

// ---------------- getters / статус ----------------

LiftState smGetState() { return state; }

uint8_t smGetCurrentFloor() { return currentFloor; }
uint8_t smGetTargetFloor()  { return targetFloor; }

long smGetCurrentPosition() { return motorGetCurrentPosition(); }

void smPrintStatus(Stream &out) {
  out.print("STATE=");
  out.print((int)state);
  out.print(" FLOOR=");
  out.print(currentFloor);
  out.print(" TARGET_FLOOR=");
  out.print(targetFloor);
  out.print(" POS=");
  out.print(motorGetCurrentPosition());
  out.print(" ERROR=");
  out.print(errorCode);
  out.println();
}

// Принудительный переход в режим NEED_CALIB (для кнопки на базе)
void smForceNeedCalib() {
  Serial.println("[SM] Force NEED_CALIB");
  state     = STATE_NEED_CALIB;
  errorCode = 0;
  targetFloor = 0;
}
