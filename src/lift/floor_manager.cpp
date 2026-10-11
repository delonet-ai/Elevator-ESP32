#include "floor_manager.h"
#include "config.h"
#include "log.h"

#include <Preferences.h>

namespace {
static_assert(!LEGACY_MIDDLE_ENABLED || FLOOR_COUNT == 3, "Legacy middle requires three floors");

// NVS: namespace и ключи. Версия нужна, чтобы прошивка с другой
// раскладкой данных не прочитала чужие значения как свои.
const char *NVS_NAMESPACE = "lift";
const char *KEY_VERSION   = "ver";
const char *KEY_TRAVEL    = "travel";
const uint32_t CALIB_STORAGE_VERSION = 2;
const char *KEY_CALIB = "geometry";
struct Geometry { uint32_t version; int32_t travel, middle; }; // one atomic NVS blob
long g_middle = 0; // zero means evenly spaced legacy floors
uint32_t g_revision = 1;
bool validTravel(long travel) { return travel >= MIN_TRAVEL_STEPS && travel <= CALIB_MAX_TRAVEL_STEPS; }
bool validMiddle(long position, long travel) {
  return position > POSITION_TOLERANCE && position < travel - POSITION_TOLERANCE;
}

Preferences g_prefs;

bool g_hasCalib        = false;
long g_fullTravelSteps = 0;
long g_floorPos[FLOOR_COUNT + 1];  // индексы 1..FLOOR_COUNT

// Пересчитать позиции этажей от текущего хода. Этажи делят ход поровну.
void recomputeFloorPositions() {
  for (uint8_t f = 1; f <= FLOOR_COUNT; f++) {
    g_floorPos[f] = (g_fullTravelSteps * (f - 1)) / (FLOOR_COUNT - 1);
  }
  if (LEGACY_MIDDLE_ENABLED && g_middle) g_floorPos[2] = g_middle;
}

bool persistTravel(long steps, long middle = 0) {
  if (!g_prefs.begin(NVS_NAMESPACE, false)) {
    LOG_E("[FLOOR] NVS open failed, calibration not saved");
    return false;
  }
  const Geometry record = {CALIB_STORAGE_VERSION, (int32_t)steps, (int32_t)middle};
  bool saved = g_prefs.putBytes(KEY_CALIB, &record, sizeof(record)) == sizeof(record);
  g_prefs.end();
  return saved;
}

}  // namespace

void floorInit() {
  for (uint8_t f = 0; f <= FLOOR_COUNT; f++) {
    g_floorPos[f] = 0;
  }

  long stored = 0;
  g_middle = 0;
  ++g_revision;
  // Открываем namespace на запись: при первом запуске его ещё нет, и
  // открытие «только на чтение» валится с NOT_FOUND, засоряя лог ошибкой.
  if (g_prefs.begin(NVS_NAMESPACE, false)) {
    // Presence takes precedence over v1 keys, even if the blob is damaged.
    // Never resurrect an obsolete calibration after clearing the new record.
    if (g_prefs.isKey(KEY_CALIB)) {
      Geometry record = {};
      if (g_prefs.getBytesLength(KEY_CALIB) == sizeof(record) &&
          g_prefs.getBytes(KEY_CALIB, &record, sizeof(record)) == sizeof(record) &&
          record.version == CALIB_STORAGE_VERSION &&
          ((record.travel == 0 && record.middle == 0) ||
           (validTravel(record.travel) && (!record.middle || validMiddle(record.middle, record.travel))))) {
        stored = record.travel;
        g_middle = record.middle;
      }
    } else if (g_prefs.getUInt(KEY_VERSION, 0) == 1) {
      stored = g_prefs.getLong(KEY_TRAVEL, 0);
    }
    g_prefs.end();
  } else {
    LOG_E("[FLOOR] NVS unavailable, calibration will not survive reboot");
  }

  if (validTravel(stored)) {
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

  if (!validTravel(steps) || !persistTravel(steps)) return false;
  g_middle = 0;
  ++g_revision;
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
  g_middle = 0;
  ++g_revision;
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

bool floorLegacyMiddleEnabled() { return LEGACY_MIDDLE_ENABLED; }
bool floorMiddleCustom() { return LEGACY_MIDDLE_ENABLED && g_middle != 0; }
uint32_t floorRevision() { return g_revision; }
bool floorCanSaveMiddle(long position) {
  return LEGACY_MIDDLE_ENABLED && floorHasValidCalibration() && validMiddle(position, g_fullTravelSteps);
}
uint8_t floorApplyMiddle(const MiddleRequest &request, uint32_t now, bool idleKnown, long currentPosition) {
  if (!LEGACY_MIDDLE_ENABLED || !idleKnown || !floorHasValidCalibration() ||
      now - request.receivedAt > 1000 || request.revision != g_revision ||
      request.position != currentPosition || (!request.reset && !floorCanSaveMiddle(currentPosition))) return 3;
  const long middle = request.reset ? 0 : currentPosition;
  if (!persistTravel(g_fullTravelSteps, middle)) return 4;
  g_middle = middle;
  ++g_revision;
  recomputeFloorPositions();
  return 2;
}
