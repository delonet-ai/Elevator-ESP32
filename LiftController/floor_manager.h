#pragma once
#include <Arduino.h>

void floorInit();

bool floorHasValidCalibration();
long floorGetPositionForFloor(uint8_t floor);
uint8_t floorGetNearestFloor(long position);

void floorSetFullTravelSteps(long steps);
long floorGetFullTravelSteps();
