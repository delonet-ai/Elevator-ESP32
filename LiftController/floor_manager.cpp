#include "floor_manager.h"

// Пока всё хранится только в RAM.
// Потом сюда добавим сохранение в NVS/EEPROM.

static bool   hasCalib = false;
static long   fullTravelSteps = 0;
static long   floorPos[4]; // 1..3

void floorInit() {
  // TODO: читать из NVS.
  hasCalib = false;
  fullTravelSteps = 0;
  floorPos[1] = 0;
  floorPos[2] = 0;
  floorPos[3] = 0;
  Serial.println("[FLOOR] Init (no calib)");
}

bool floorHasValidCalibration() {
  return hasCalib && fullTravelSteps > 0;
}

long floorGetPositionForFloor(uint8_t floor) {
  if (floor < 1 || floor > 3) return 0;
  return floorPos[floor];
}

uint8_t floorGetNearestFloor(long position) {
  if (!floorHasValidCalibration()) return 0;
  long d1 = labs(position - floorPos[1]);
  long d2 = labs(position - floorPos[2]);
  long d3 = labs(position - floorPos[3]);
  if (d1 <= d2 && d1 <= d3) return 1;
  if (d2 <= d1 && d2 <= d3) return 2;
  return 3;
}

void floorSetFullTravelSteps(long steps) {
  fullTravelSteps = steps;
  if (steps <= 0) {
    hasCalib = false;
    return;
  }
  floorPos[1] = 0;
  floorPos[3] = fullTravelSteps;
  floorPos[2] = fullTravelSteps / 2;
  hasCalib = true;

  Serial.print("[FLOOR] Calibrated: full=");
  Serial.print(fullTravelSteps);
  Serial.print(" floor1=");
  Serial.print(floorPos[1]);
  Serial.print(" floor2=");
  Serial.print(floorPos[2]);
  Serial.print(" floor3=");
  Serial.println(floorPos[3]);
}

long floorGetFullTravelSteps() {
  return fullTravelSteps;
}
