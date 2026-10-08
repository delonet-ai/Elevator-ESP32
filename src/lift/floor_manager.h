#pragma once
#include <Arduino.h>

// Геометрия шахты и позиции этажей.
//
// Система координат: 0 — уровень 1-го этажа (низ), ось направлена вверх.
// Верхний концевик находится на floorGetTopSwitchPosition(), то есть выше
// верхнего этажа на TOP_MARGIN_STEPS.
//
// Калибровка хранится в NVS и переживает перезагрузку.

void floorInit();

bool floorHasValidCalibration();

// Позиция этажа (1..FLOOR_COUNT) в шагах. Для неверного номера — 0.
long floorGetPositionForFloor(uint8_t floor);

// Ближайший этаж к позиции; 0, если калибровки нет.
uint8_t floorGetNearestFloor(long position);

// Записать измеренный ход и пересчитать этажи. Сохраняется в NVS.
// steps <= 0 трактуется как «калибровки нет».
bool floorSetFullTravelSteps(long steps);

// Стереть калибровку (в том числе из NVS).
bool floorClearCalibration();

long floorGetFullTravelSteps();

// Позиция верхнего концевика в той же системе координат.
long floorGetTopSwitchPosition();
