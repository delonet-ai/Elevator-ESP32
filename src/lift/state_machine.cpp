#include "state_machine.h"
#include "web_config.h"
#include "web_calibration.h"
#include "motor_controller.h"
#include "io_manager.h"
#include "floor_manager.h"
#include "calibration_manager.h"
#include "config.h"
#include "motion_settings.h"
#include "event_journal.h"
#include "log.h"

namespace {

LiftState g_state        = STATE_BOOT;
LiftError g_error        = ERR_NONE;
uint8_t   g_targetFloor  = 0;
long      g_targetPos    = 0;

// Позиция кабины известна только после хоминга или калибровки.
// После перезагрузки лифт не знает, где он стоит, и раньше молча
// считал себя на 1-м этаже.
bool g_positionKnown = false;

// Удержание кнопки на пульте. Пульт повторяет команду, пока кнопка нажата;
// если повторы пропали, база останавливается сама.
unsigned long g_holdDeadline   = 0;
unsigned long g_motionStartMs  = 0;
unsigned long g_motionTimeoutMs = MOTION_TIMEOUT_MIN_MS;

// Направление, которое мы САМИ скомандовали в ручном режиме.
// Опрашивать motorGetDirection() здесь нельзя: сразу после команды мотор
// ещё не поехал и вернёт 0, из-за чего движение перезапускалось бы
// на каждом повторе удержания, обнуляя MANUAL_MAX_DURATION_MS.
int g_manualDir = 0;
// То же для спуска в калибровке: motorIsRunning() сразу после команды
// ещё false, и повтор удержания перезапускал бы движение.
bool g_calibDescending = false;

// Пауза после старта движения, в течение которой «мотор стоит» не считается
// отказом: периферии нужно время, чтобы подхватить команду.
const unsigned long MOTION_SETTLE_MS = 250;

void setState(LiftState state) {
  if (g_state != state) journalRecord(EventKind::State, state);
  g_state = state;
}

void enterError(LiftError code, const char *why) {
  motorStopHard();
  journalRecord(EventKind::Error, code);
  setState(STATE_ERROR);
  g_error           = code;
  g_manualDir       = 0;
  g_calibDescending = false;
  g_targetFloor = 0;
  g_holdDeadline = 0;
  LOG_E("[SM] ERROR %u: %s", (unsigned)code, why);
}

void enterIdle() {
  setState(STATE_IDLE);
  g_manualDir       = 0;
  g_calibDescending = false;
  g_targetFloor  = 0;
  g_holdDeadline = 0;
}

// Куда перейти, когда движение закончено, а ошибок нет.
void enterReadyState() {
  g_manualDir       = 0;
  g_calibDescending = false;
  if (!calibIsValid()) {
    setState(STATE_NEED_CALIB);
  } else if (!g_positionKnown) {
    setState(STATE_NEED_HOMING);
  } else {
    setState(STATE_IDLE);
  }
  g_targetFloor  = 0;
  g_holdDeadline = 0;
}

bool isCalibState(LiftState s) {
  return s == STATE_NEED_CALIB ||
         s == STATE_CALIB_HOMING_UP ||
         s == STATE_CALIB_MOVING_DOWN;
}

// Концевик достигнут. Реакция зависит от того, зачем мы ехали вверх.
void handleTopSwitch() {
  switch (g_state) {
    case STATE_CALIB_HOMING_UP:
      calibMarkTop();  // сама останавливает привод и ждёт останова
      setState(STATE_CALIB_MOVING_DOWN);
      g_holdDeadline = 0;
      LOG_I("[SM] Calibration: top reached, waiting for DOWN");
      break;

    case STATE_HOMING:
      motorStopHardAndWait();
      // Концевик — единственная физически достоверная точка шахты.
      motorSetPosition(floorGetTopSwitchPosition());
      g_positionKnown = true;
      enterIdle();
      LOG_I("[SM] Homing done, position = %ld", motorGetPosition());
      break;

    default:
      // Во всех остальных состояниях концевик наверху — аварийная ситуация:
      // либо калибровка неверна, либо кабину увели выше предела.
      enterError(ERR_TOP_LIMIT, "top limit switch hit unexpectedly");
      break;
  }
}

// Программные пределы хода. Работают только когда позиция достоверна.
void checkSoftLimits() {
  if (!g_positionKnown || !calibIsValid()) return;
  if (g_state != STATE_MOVING && g_state != STATE_MANUAL_MOVE) return;

  int dir = motorGetDirection();
  if (dir == 0) return;

  long pos = motorGetPosition();
  if (dir > 0 && pos > floorGetTopSwitchPosition() + SOFT_LIMIT_MARGIN_STEPS) {
    enterError(ERR_SOFT_LIMIT, "soft limit above top");
  } else if (dir < 0 && pos < -SOFT_LIMIT_MARGIN_STEPS) {
    enterError(ERR_SOFT_LIMIT, "soft limit below bottom");
  }
}

// Лимит хода в калибровке — страховка на случай неисправного концевика.
void checkCalibTravelLimit() {
  if (g_state != STATE_CALIB_HOMING_UP && g_state != STATE_CALIB_MOVING_DOWN) return;
  if (!motorIsRunning()) return;

  if (labs(motorGetPosition()) > CALIB_MAX_TRAVEL_STEPS) {
    enterError(ERR_CALIB_TIMEOUT, "calibration travel limit exceeded");
  }
}

// Общий разбор команды удержания: продлевает дедлайн и при первом вызове
// запускает движение.
void holdRefresh() {
  g_holdDeadline = millis() + MANUAL_HOLD_TIMEOUT_MS;
}

}  // namespace

// ---------------------------------------------------------------- init/tick

void smInit() {
  journalRecord(EventKind::Boot, 0);
  g_error         = ERR_NONE;
  g_targetFloor   = 0;
  g_positionKnown = false;

  if (!calibIsValid()) {
    setState(STATE_NEED_CALIB);
    LOG_I("[SM] No calibration -> NEED_CALIB");
    return;
  }

  // Калибровка есть. Если кабина уже стоит на концевике, позиция известна
  // сразу и хоминг не нужен.
  if (ioTopSwitchActive()) {
    motorSetPosition(floorGetTopSwitchPosition());
    g_positionKnown = true;
    setState(STATE_IDLE);
    LOG_I("[SM] Calibration OK, cabin at top switch -> IDLE");
  } else {
    setState(STATE_NEED_HOMING);
    LOG_I("[SM] Calibration OK, position unknown -> NEED_HOMING");
  }
}

void smFastPoll() {
  // 1. Концевик. Проверяем при каждом проходе loop().
  if (ioTopSwitchActive() && motorGetDirection() > 0) {
    handleTopSwitch();
    return;
  }
  // Концевик мог сработать уже после остановки — например, по инерции.
  if (ioTopSwitchActive() &&
      (g_state == STATE_CALIB_HOMING_UP || g_state == STATE_HOMING)) {
    handleTopSwitch();
    return;
  }

  // 2. Программные пределы.
  checkSoftLimits();

  // 3. Лимит хода в калибровке.
  checkCalibTravelLimit();

  // 4. Дедлайн удержания кнопки: пропали повторы — останавливаемся.
  if (g_holdDeadline != 0 && (long)(millis() - g_holdDeadline) >= 0) {
    g_holdDeadline = 0;
    if (g_state == STATE_MANUAL_MOVE) {
      motorStopSmooth();
      enterReadyState();
      LOG_W("[SM] Manual hold expired, stopping");
    } else if (g_state == STATE_CALIB_MOVING_DOWN) {
      motorStopSmooth();
      g_calibDescending = false;
      LOG_W("[SM] Calibration hold expired, stopping descent");
    }
  }
}

void smTick() {
  // Потенциометр задаёт предел скорости автоматических поездок.
  motorSetSpeedLimit(motionSettings().minimum + (motionSettings().maximum - motionSettings().minimum) *
                                 ((float)ioSpeedPercent() / 100.0f));

  unsigned long now = millis();

  switch (g_state) {
    case STATE_BOOT:
    case STATE_NEED_CALIB:
    case STATE_NEED_HOMING:
    case STATE_IDLE:
    case STATE_ERROR:
      break;

    case STATE_HOMING:
    case STATE_CALIB_HOMING_UP:
      if (now - g_motionStartMs > HOMING_TIMEOUT_MS) {
        enterError(g_state == STATE_HOMING ? ERR_MOTION_TIMEOUT : ERR_CALIB_TIMEOUT,
                   "top switch not reached in time");
      }
      break;

    case STATE_CALIB_MOVING_DOWN:
      if (motorIsRunning() && (now - g_motionStartMs > CALIB_DOWN_TIMEOUT_MS)) {
        enterError(ERR_CALIB_TIMEOUT, "calibration descent timeout");
      }
      break;

    case STATE_MOVING: {
      long pos  = motorGetPosition();
      long dist = labs(g_targetPos - pos);

      if (dist <= POSITION_TOLERANCE && !motorIsRunning()) {
        motorStopSmooth();
        LOG_I("[SM] Arrived at floor %u (pos %ld)", g_targetFloor, pos);
        enterIdle();
        break;
      }

      // Мотор встал, а до цели далеко — значит движение сорвано.
      // Первые MOTION_SETTLE_MS не учитываем: команда могла ещё не дойти
      // до периферии, и isRunning() законно возвращает false.
      if (!motorIsRunning() && dist > POSITION_TOLERANCE &&
          (now - g_motionStartMs) > MOTION_SETTLE_MS) {
        enterError(ERR_MOTION_TIMEOUT, "motor stopped before reaching target");
        break;
      }

      if (now - g_motionStartMs > g_motionTimeoutMs) {
        enterError(ERR_MOTION_TIMEOUT, "motion timeout");
      }
      break;
    }

    case STATE_MANUAL_MOVE:
      if (now - g_motionStartMs > MANUAL_MAX_DURATION_MS) {
        motorStopSmooth();
        enterReadyState();
        LOG_W("[SM] Manual move exceeded %lu ms, stopped",
              (unsigned long)MANUAL_MAX_DURATION_MS);
      }
      break;
  }
}

// ----------------------------------------------------------------- команды

void smCommandMoveToFloor(uint8_t floor) {
  if (webMotionLocked()) return;
  if (floor < 1 || floor > FLOOR_COUNT) {
    LOG_W("[SM] Invalid floor %u", floor);
    return;
  }
  if (isCalibState(g_state)) {
    LOG_W("[SM] Floor call ignored: calibration required or in progress");
    return;
  }
  if (g_state == STATE_ERROR) {
    LOG_W("[SM] Floor call ignored: error state");
    return;
  }
  if (!calibIsValid()) {
    LOG_W("[SM] Floor call ignored: not calibrated");
    return;
  }
  if (!g_positionKnown) {
    // Ехать «на этаж», не зная, где кабина, — верный способ въехать в упор.
    LOG_W("[SM] Floor call ignored: position unknown, homing required");
    if (g_error != ERR_NO_HOME) journalRecord(EventKind::Error, ERR_NO_HOME);
    g_error = ERR_NO_HOME;
    return;
  }
  if (g_state != STATE_IDLE && g_state != STATE_MOVING) {
    LOG_W("[SM] Floor call ignored in state %u", (unsigned)g_state);
    return;
  }

  long pos  = motorGetPosition();
  long dest = floorGetPositionForFloor(floor);

  if (labs(dest - pos) <= POSITION_TOLERANCE) {
    LOG_I("[SM] Already at floor %u", floor);
    enterIdle();
    return;
  }

  g_error         = ERR_NONE;
  g_targetFloor   = floor;
  g_targetPos     = dest;
  setState(STATE_MOVING);
  g_motionStartMs = millis();
  g_holdDeadline  = 0;

  // Худший случай — регулятор скорости выкручен в минимум.
  unsigned long expected =
      (unsigned long)((labs(dest - pos) * 1000.0f / motionSettings().minimum) * MOTION_TIMEOUT_FACTOR);
  // Allow both ramps when a low acceleration is selected in the web UI.
  const unsigned long rampAllowance = 2000UL * motionSettings().maximum / motionSettings().acceleration;
  g_motionTimeoutMs = expected + rampAllowance + MOTION_TIMEOUT_MIN_MS;

  motorMoveTo(dest);
  LOG_I("[SM] Moving to floor %u (pos %ld -> %ld)", floor, pos, dest);
}

void smCommandStop() {
  journalRecord(EventKind::Stop, 0);
  const bool browserOwned = webCalibrationBlocksCommands();
  webCalibrationExternalStop();
  // Экстренный стоп обязан работать в любом состоянии, включая калибровку.
  motorStopHard();
  g_holdDeadline = 0;
  g_targetFloor  = 0;
  g_calibDescending = false;

  if (g_state == STATE_MOVING || g_state == STATE_MANUAL_MOVE ||
      g_state == STATE_HOMING) {
    // После резкой остановки шаги могли быть потеряны — позиции больше
    // не доверяем, пока не пройдёт хоминг.
    g_positionKnown = false;
    enterReadyState();
  } else if (g_state == STATE_CALIB_HOMING_UP) {
    setState(STATE_NEED_CALIB);
  } else if (browserOwned && g_state == STATE_CALIB_MOVING_DOWN) {
    setState(STATE_NEED_CALIB);
  }
  // Для пульта спуск можно продолжить; веб-сессия после STOP отзывается.
  LOG_I("[SM] STOP");
}

void smCommandAbortCalib() {
  smCommandStop();
  if (g_state == STATE_CALIB_MOVING_DOWN) setState(STATE_NEED_CALIB);
}

void smCommandStartCalib() {
  if (webMotionLocked()) return;
  if (g_state == STATE_MOVING || g_state == STATE_MANUAL_MOVE ||
      g_state == STATE_HOMING) {
    LOG_W("[SM] Cannot start calibration while moving");
    return;
  }

  g_error         = ERR_NONE;
  g_positionKnown = false;
  g_targetFloor   = 0;
  setState(STATE_CALIB_HOMING_UP);
  g_calibDescending = false;
  g_motionStartMs   = millis();
  if (ioTopSwitchActive()) {
    handleTopSwitch();
    return;
  }
  motorRunUp(motionSettings().homing);
  LOG_I("[SM] Calibration started: homing up");
}

void smCommandCalibDownHold() {
  if (webMotionLocked()) return;
  if (g_state != STATE_CALIB_MOVING_DOWN) {
    LOG_W("[SM] Calib DOWN ignored in state %u", (unsigned)g_state);
    return;
  }
  if (!g_calibDescending) {
    g_calibDescending = true;
    g_motionStartMs   = millis();
    motorRunDown(motionSettings().down);
  }
  holdRefresh();
}

void smCommandCalibDownSave() {
  if (webMotionLocked()) return;
  if (g_state != STATE_CALIB_MOVING_DOWN) {
    LOG_W("[SM] Calib SAVE ignored in state %u", (unsigned)g_state);
    return;
  }

  // Низ фиксируем по факту останова, а не по команде торможения:
  // за время замедления кабина проехала бы ещё сотни шагов.
  motorStopHardAndWait();
  g_holdDeadline    = 0;
  g_calibDescending = false;

  if (motorGetPosition() > -(TOP_MARGIN_STEPS + MIN_TRAVEL_STEPS)) {
    enterError(ERR_TRAVEL_TOO_SHORT, "measured travel too short");
    return;
  }
  if (!calibFinishAtBottom(motorGetPosition())) {
    enterError(ERR_STORAGE, "calibration could not be saved");
    return;
  }

  g_positionKnown = true;
  g_error         = ERR_NONE;
  enterIdle();
  LOG_I("[SM] Calibration finished, lift ready");
}

void smCommandManualUpHold() {
  if (webMotionLocked()) return;
  // Из NEED_HOMING кнопка «вверх» запускает хоминг: ехать вверх безопасно,
  // концевик всё равно остановит.
  if (g_state == STATE_NEED_HOMING) {
    smCommandStartHoming();
    return;
  }
  if (g_state == STATE_NEED_CALIB) {
    smCommandStartCalib();
    return;
  }
  if (g_state == STATE_ERROR || isCalibState(g_state) || g_state == STATE_HOMING) {
    return;
  }

  if (g_state != STATE_MANUAL_MOVE || g_manualDir != 1) {
    setState(STATE_MANUAL_MOVE);
    g_manualDir     = 1;
    g_targetFloor   = 0;
    g_motionStartMs = millis();
    motorRunUp(motionSettings().manual);
  }
  holdRefresh();
}

void smCommandManualDownHold() {
  if (webMotionLocked()) return;
  if (g_state == STATE_CALIB_MOVING_DOWN) {
    smCommandCalibDownHold();
    return;
  }
  if (g_state == STATE_ERROR || isCalibState(g_state) || g_state == STATE_HOMING) {
    return;
  }
  if (!g_positionKnown && calibIsValid()) {
    // Вниз без известной позиции ехать нельзя: снизу концевика нет,
    // упор находит только трос.
    LOG_W("[SM] Manual DOWN ignored: position unknown, home first");
    return;
  }

  if (g_state != STATE_MANUAL_MOVE || g_manualDir != -1) {
    setState(STATE_MANUAL_MOVE);
    g_manualDir     = -1;
    g_targetFloor   = 0;
    g_motionStartMs = millis();
    motorRunDown(motionSettings().manual);
  }
  holdRefresh();
}

void smCommandManualStop() {
  // A stop from the remote/USB also revokes the browser's lease.
  if (webCalibrationBlocksCommands()) { smCommandStop(); return; }
  g_holdDeadline = 0;

  if (g_state == STATE_MANUAL_MOVE) {
    motorStopSmooth();
    enterReadyState();
    LOG_I("[SM] Manual stop");
  } else if (g_state == STATE_CALIB_MOVING_DOWN && g_calibDescending) {
    motorStopSmooth();
    g_calibDescending = false;
    LOG_I("[SM] Calibration descent stopped");
  }
}

void smCommandStartHoming() {
  if (webMotionLocked()) return;
  if (!calibIsValid()) {
    LOG_W("[SM] Homing ignored: not calibrated");
    return;
  }
  if (g_state == STATE_MOVING || g_state == STATE_MANUAL_MOVE ||
      isCalibState(g_state)) {
    LOG_W("[SM] Homing ignored in state %u", (unsigned)g_state);
    return;
  }

  g_error         = ERR_NONE;
  g_targetFloor   = 0;
  setState(STATE_HOMING);
  g_motionStartMs = millis();
  motorRunUp(motionSettings().homing);
  LOG_I("[SM] Homing up to top switch");
}

void smCommandClearError() {
  if (webMotionLocked()) return;
  if (g_state != STATE_ERROR) return;
  journalRecord(EventKind::Error, ERR_NONE);
  g_error = ERR_NONE;
  // После любой ошибки позиция считается недостоверной.
  g_positionKnown = false;
  enterReadyState();
  LOG_I("[SM] Error cleared -> state %u", (unsigned)g_state);
}

void smForceNeedCalib() {
  if (webCalibrationBlocksCommands()) { smCommandStop(); return; }
  motorStopHard();
  g_positionKnown = false;
  if (!calibReset()) {
    enterError(ERR_STORAGE, "calibration could not be cleared");
    return;
  }
  g_error         = ERR_NONE;
  g_targetFloor   = 0;
  g_positionKnown = false;
  g_holdDeadline  = 0;
  setState(STATE_NEED_CALIB);
  LOG_I("[SM] Forced NEED_CALIB");
}

// ------------------------------------------------------------------ статус

LiftState smGetState() { return g_state; }

uint8_t smGetCurrentFloor() {
  if (!g_positionKnown || !calibIsValid()) return 0;
  return floorGetNearestFloor(motorGetPosition());
}

uint8_t smGetTargetFloor() { return g_targetFloor; }

int8_t smGetDirection() { return (int8_t)motorGetDirection(); }

uint8_t smGetError() { return (uint8_t)g_error; }

long smGetPosition() { return motorGetPosition(); }

bool smPositionKnown() { return g_positionKnown; }

void smPrintStatus(Stream &out) {
  out.printf("STATE=%u FLOOR=%u TARGET=%u POS=%ld KNOWN=%u DIR=%d ERR=%u TRAVEL=%ld\n",
             (unsigned)g_state, smGetCurrentFloor(), g_targetFloor,
             motorGetPosition(), g_positionKnown ? 1 : 0,
             motorGetDirection(), (unsigned)g_error,
             floorGetFullTravelSteps());
}

#ifdef BENCH_COMMANDS
void smBenchSetCalibrated(long travelSteps) {
  motorStopHardAndWait();
  if (!floorSetFullTravelSteps(travelSteps)) {
    enterError(ERR_STORAGE, "bench calibration could not be saved");
    return;
  }
  motorSetPosition(0);
  g_positionKnown = true;
  g_error         = ERR_NONE;
  g_manualDir     = 0;
  enterIdle();
  LOG_W("[SM] BENCH: calibration injected, travel=%ld", travelSteps);
}
#endif
