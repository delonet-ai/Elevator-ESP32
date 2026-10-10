#include "floor_manager.h"
#include "config.h"
#include "log.h"

#include <Preferences.h>

namespace {

// NVS: namespace и ключи. Версия нужна, чтобы прошивка с другой
// раскладкой данных не прочитала чужие значения как свои.
const char *NVS_NAMESPACE = "lift";
const char *KEY_VERSION   = "ver";
const char *KEY_TRAVEL    = "travel";
const uint32_t CALIB_STORAGE_VERSION = 1;

Preferences g_prefs;

bool g_hasCalib        = false;
long g_fullTravelSteps = 0;
long g_floorPos[FLOOR_COUNT + 1];  // индексы 1..FLOOR_COUNT

// Пересчитать позиции этажей от текущего хода. Этажи делят ход поровну.
void recomputeFloorPositions() {
  for (uint8_t f = 1; f <= FLOOR_COUNT; f++) {
    g_floorPos[f] = (g_fullTravelSteps * (f - 1)) / (FLOOR_COUNT - 1);
  }
}

bool persistTravel(long steps) {
  if (!g_prefs.begin(NVS_NAMESPACE, false)) {
    LOG_E("[FLOOR] NVS open failed, calibration not saved");
    return false;
  }
  bool saved = g_prefs.putUInt(KEY_VERSION, CALIB_STORAGE_VERSION) == sizeof(uint32_t) &&
               g_prefs.putLong(KEY_TRAVEL, steps) == sizeof(int32_t);
  g_prefs.end();
  return saved;
}

}  // namespace

void floorInit() {
  for (uint8_t f = 0; f <= FLOOR_COUNT; f++) {
    g_floorPos[f] = 0;
  }

  long stored = 0;
  // Открываем namespace на запись: при первом запуске его ещё нет, и
  // открытие «только на чтение» валится с NOT_FOUND, засоряя лог ошибкой.
  if (g_prefs.begin(NVS_NAMESPACE, false)) {
    uint32_t ver = g_prefs.getUInt(KEY_VERSION, 0);
    if (ver == CALIB_STORAGE_VERSION) {
      stored = g_prefs.getLong(KEY_TRAVEL, 0);
    } else if (ver != 0) {
      LOG_W("[FLOOR] NVS layout v%u != v%u, stored calibration ignored",
            (unsigned)ver, (unsigned)CALIB_STORAGE_VERSION);
    }
    g_prefs.end();
  } else {
    LOG_E("[FLOOR] NVS unavailable, calibration will not survive reboot");
  }

  if (stored >= MIN_TRAVEL_STEPS) {
    g_fullTravelSteps = stored;
    g_hasCalib        = true;
    recomputeFloorPositions();
    LOG_I("[FLOOR] Loaded calibration: travel=%ld", g_fullTravelSteps);
  } else {
    g_fullTravelSteps = 0;
    g_hasCalib        = false;
    LOG_I("[FLOOR] No stored calibration");
  }
}

bool floorHasValidCalibration() {
  return g_hasCalib && g_fullTravelSteps >= MIN_TRAVEL_STEPS;
}

long floorGetPositionForFloor(uint8_t floor) {
  if (floor < 1 || floor > FLOOR_COUNT) return 0;
  return g_floorPos[floor];
}

uint8_t floorGetNearestFloor(long position) {
  if (!floorHasValidCalibration()) return 0;

  uint8_t best     = 1;
  long    bestDist = labs(position - g_floorPos[1]);
  for (uint8_t f = 2; f <= FLOOR_COUNT; f++) {
    long d = labs(position - g_floorPos[f]);
    if (d < bestDist) {
      bestDist = d;
      best     = f;
    }
  }
  return best;
}

bool floorSetFullTravelSteps(long steps) {
  if (steps < MIN_TRAVEL_STEPS) {
    return floorClearCalibration();
  }

  if (!persistTravel(steps)) return false;
  g_fullTravelSteps = steps;
  g_hasCalib        = true;
  recomputeFloorPositions();

  LOG_I("[FLOOR] Calibrated: travel=%ld f1=%ld f2=%ld f3=%ld topSwitch=%ld",
        g_fullTravelSteps, g_floorPos[1], g_floorPos[2], g_floorPos[3],
        floorGetTopSwitchPosition());
  return true;
}

bool floorClearCalibration() {
  if (!persistTravel(0)) return false;
  g_fullTravelSteps = 0;
  g_hasCalib        = false;
  for (uint8_t f = 0; f <= FLOOR_COUNT; f++) {
    g_floorPos[f] = 0;
  }
  LOG_I("[FLOOR] Calibration cleared");
  return true;
}

long floorGetFullTravelSteps() {
  return g_fullTravelSteps;
}

long floorGetTopSwitchPosition() {
  return g_fullTravelSteps + TOP_MARGIN_STEPS;
}
