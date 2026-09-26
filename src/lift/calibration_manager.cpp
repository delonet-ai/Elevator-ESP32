#include "calibration_manager.h"
#include "floor_manager.h"
#include "motor_controller.h"
#include "config.h"
#include "log.h"

void calibInit() {
  LOG_I("[CALIB] Init, stored calibration %s",
        floorHasValidCalibration() ? "found" : "absent");
}

bool calibIsValid() {
  return floorHasValidCalibration();
}

void calibMarkTop() {
  // Калибровочная шкала: 0 на концевике, вниз — отрицательные значения.
  motorSetPosition(0);
  LOG_I("[CALIB] Top switch reached, calibration zero set");
}

bool calibFinishAtBottom(long bottomPos) {
  long measured = labs(bottomPos);
  long travel   = measured - TOP_MARGIN_STEPS;

  LOG_I("[CALIB] Bottom at %ld, switch-to-bottom = %ld, travel = %ld",
        bottomPos, measured, travel);

  if (travel < MIN_TRAVEL_STEPS) {
    // Раньше слишком короткий ход молча подменялся на минимальный, и лифт
    // уезжал в «этажи», которых нет. Теперь это ошибка калибровки.
    LOG_E("[CALIB] Travel %ld < minimum %ld, calibration rejected",
          travel, MIN_TRAVEL_STEPS);
    return false;
  }

  floorSetFullTravelSteps(travel);

  // Переходим в рабочую шкалу: низ = 0, ось вверх.
  motorSetPosition(0);

  LOG_I("[CALIB] Done. Floor1=%ld Floor2=%ld Floor3=%ld, top switch at %ld",
        floorGetPositionForFloor(1), floorGetPositionForFloor(2),
        floorGetPositionForFloor(3), floorGetTopSwitchPosition());
  return true;
}

void calibReset() {
  floorClearCalibration();
  LOG_I("[CALIB] Calibration reset");
}
