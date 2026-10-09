#include "motor_controller.h"
#include "pins.h"
#include "config.h"
#include "motion_settings.h"
#include "log.h"

#include <FastAccelStepper.h>

namespace {

FastAccelStepperEngine g_engine = FastAccelStepperEngine();
FastAccelStepper      *g_stepper = nullptr;

float g_speedLimit = SPEED_MAX;
int   g_direction  = 0;     // направление последней выданной команды
bool  g_autoMove   = false; // true — едем в заданную позицию, а не "до стопа"

// Скорость, с которой сейчас идёт движение. Отдельно от g_speedLimit,
// потому что ручной режим и хоминг ездят на своих скоростях.
uint32_t g_activeSpeedHz = 0;

uint32_t clampSpeed(float stepsPerSec) {
  if (stepsPerSec < SPEED_MIN) stepsPerSec = SPEED_MIN;
  if (stepsPerSec > SPEED_MAX) stepsPerSec = SPEED_MAX;
  return (uint32_t)stepsPerSec;
}

// Почему не forceStop(): он выставляет в очереди флаг ignore_commands и
// не снимает его, из-за чего следующий runForward()/runBackward() молча
// не запускается — команда принимается, а привод стоит.
// forceStopAndNewPosition() останавливает так же резко, но корректно
// сбрасывает очередь.
void forceStopKeepingPosition() {
  g_stepper->forceStopAndNewPosition(g_stepper->getCurrentPosition());
  g_direction = 0;
  g_autoMove  = false;
}

}  // namespace

bool motorInit() {
  g_engine.init();

  g_stepper = g_engine.stepperConnectToPin(PIN_STEP);
  if (g_stepper == nullptr) {
    LOG_E("[MOTOR] stepperConnectToPin(%u) failed", PIN_STEP);
    return false;
  }

  // DIR HIGH соответствует движению вверх (рост позиции).
  g_stepper->setDirectionPin(PIN_DIR, true);
  // Драйвер включается низким уровнем на EN.
  g_stepper->setEnablePin(PIN_EN, true);
  // Автоотключение выключено: удерживающий момент нужен постоянно,
  // иначе кабина под своим весом сползает вниз.
  g_stepper->setAutoEnable(false);
  g_stepper->enableOutputs();

  g_speedLimit = motionSettings().minimum;
  g_stepper->setAcceleration((int32_t)motionSettings().acceleration);
  g_stepper->setSpeedInHz(clampSpeed(g_speedLimit));
  g_stepper->setCurrentPosition(0);

  LOG_I("[MOTOR] Init OK: STEP=%u DIR=%u EN=%u", PIN_STEP, PIN_DIR, PIN_EN);
  return true;
}

void motorSetSpeedLimit(float stepsPerSec) {
  uint32_t hz = clampSpeed(stepsPerSec);
  if ((uint32_t)g_speedLimit == hz) return;
  g_speedLimit = (float)hz;

  // Потенциометр ограничивает только автоматические поездки. У ручного
  // движения, хоминга и калибровки скорости заданы отдельно, и перебивать
  // их регулятором нельзя: это ломает предсказуемость калибровки.
  if (g_stepper != nullptr && g_autoMove && g_stepper->isRunning()) {
    g_activeSpeedHz = hz;
    g_stepper->setSpeedInHz(hz);
    g_stepper->applySpeedAcceleration();
  }
}

void motorSetAccel(float stepsPerSec2) {
  if (stepsPerSec2 < 10.0f) stepsPerSec2 = 10.0f;
  if (g_stepper != nullptr) {
    g_stepper->setAcceleration((int32_t)stepsPerSec2);
  }
}

void motorMoveTo(long position) {
  if (g_stepper == nullptr) return;

  g_activeSpeedHz = clampSpeed(g_speedLimit);
  g_stepper->setSpeedInHz(g_activeSpeedHz);

  long current = g_stepper->getCurrentPosition();
  g_direction = (position > current) ? 1 : ((position < current) ? -1 : 0);

  g_autoMove = true;
  int8_t res = g_stepper->moveTo((int32_t)position);
  if (res != MOVE_OK) {
    // Код возврата раньше игнорировался, и отказ привода выглядел как
    // «команда принята, но кабина не едет».
    LOG_E("[MOTOR] moveTo rejected, code %d", (int)res);
    g_direction = 0;
    g_autoMove  = false;
    return;
  }
  LOG_I("[MOTOR] MoveTo %ld from %ld at %u Hz", position, current, (unsigned)g_activeSpeedHz);
}

void motorRunUp(float stepsPerSec) {
  if (g_stepper == nullptr) return;
  g_activeSpeedHz = clampSpeed(stepsPerSec);
  g_stepper->setSpeedInHz(g_activeSpeedHz);
  g_direction = 1;
  g_autoMove  = false;
  int8_t res = g_stepper->runForward();
  if (res != MOVE_OK) {
    LOG_E("[MOTOR] runForward rejected, code %d", (int)res);
    g_direction = 0;
    return;
  }
  LOG_I("[MOTOR] Run UP at %u Hz", (unsigned)g_activeSpeedHz);
}

void motorRunDown(float stepsPerSec) {
  if (g_stepper == nullptr) return;
  g_activeSpeedHz = clampSpeed(stepsPerSec);
  g_stepper->setSpeedInHz(g_activeSpeedHz);
  g_direction = -1;
  g_autoMove  = false;
  int8_t res = g_stepper->runBackward();
  if (res != MOVE_OK) {
    LOG_E("[MOTOR] runBackward rejected, code %d", (int)res);
    g_direction = 0;
    return;
  }
  LOG_I("[MOTOR] Run DOWN at %u Hz", (unsigned)g_activeSpeedHz);
}

void motorStopSmooth() {
  if (g_stepper == nullptr) return;
  g_stepper->stopMove();
  g_direction = 0;
  g_autoMove  = false;
  LOG_I("[MOTOR] Stop (decelerate)");
}

void motorStopHard() {
  if (g_stepper == nullptr) return;
  forceStopKeepingPosition();
  LOG_W("[MOTOR] Emergency stop at %ld", (long)g_stepper->getCurrentPosition());
}

void motorStopHardAndWait() {
  if (g_stepper == nullptr) return;

  forceStopKeepingPosition();

  // Хвост очереди — доли миллисекунды; потолок нужен только на случай,
  // если периферия по какой-то причине не отдаёт признак останова.
  const unsigned long deadline = millis() + 50;
  while (g_stepper->isRunning() && (long)(millis() - deadline) < 0) {
    delayMicroseconds(200);
  }
  LOG_W("[MOTOR] Emergency stop at %ld", (long)g_stepper->getCurrentPosition());
}

long motorGetPosition() {
  if (g_stepper == nullptr) return 0;
  return (long)g_stepper->getCurrentPosition();
}

void motorSetPosition(long position) {
  if (g_stepper == nullptr) return;
  // setCurrentPosition() на движущемся приводе оставляет генератор рампы
  // в несогласованном состоянии: следующая команда движения молча
  // отклоняется. Поэтому сначала гарантируем полный останов.
  if (g_stepper->isRunning()) {
    motorStopHardAndWait();
  }
  g_stepper->setCurrentPosition((int32_t)position);
  LOG_I("[MOTOR] Position set to %ld", position);
}

bool motorIsRunning() {
  if (g_stepper == nullptr) return false;
  return g_stepper->isRunning();
}

int motorGetDirection() {
  if (g_stepper == nullptr || !g_stepper->isRunning()) return 0;
  return g_direction;
}
