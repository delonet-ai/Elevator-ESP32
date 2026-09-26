#include "motor_controller.h"
#include "pins.h"
#include "config.h"
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

  g_stepper->setAcceleration((int32_t)ACCEL_STEPS_PER_S2);
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
  g_stepper->moveTo((int32_t)position);
  LOG_I("[MOTOR] MoveTo %ld from %ld at %u Hz", position, current, (unsigned)g_activeSpeedHz);
}

void motorRunUp(float stepsPerSec) {
  if (g_stepper == nullptr) return;
  g_activeSpeedHz = clampSpeed(stepsPerSec);
  g_stepper->setSpeedInHz(g_activeSpeedHz);
  g_direction = 1;
  g_autoMove  = false;
  g_stepper->runForward();
  LOG_I("[MOTOR] Run UP at %u Hz", (unsigned)g_activeSpeedHz);
}

void motorRunDown(float stepsPerSec) {
  if (g_stepper == nullptr) return;
  g_activeSpeedHz = clampSpeed(stepsPerSec);
  g_stepper->setSpeedInHz(g_activeSpeedHz);
  g_direction = -1;
  g_autoMove  = false;
  g_stepper->runBackward();
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
  g_stepper->forceStop();
  g_direction = 0;
  g_autoMove  = false;
  LOG_W("[MOTOR] Emergency stop");
}

long motorGetPosition() {
  if (g_stepper == nullptr) return 0;
  return (long)g_stepper->getCurrentPosition();
}

void motorSetPosition(long position) {
  if (g_stepper == nullptr) return;
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
