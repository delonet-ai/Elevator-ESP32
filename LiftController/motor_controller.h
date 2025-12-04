#pragma once
#include <Arduino.h>

// Реальный контроллер шагового мотора с STEP/DIR/EN и профилем скорости

void motorInit();
void motorService();

void motorMoveTo(long targetPosition);
void motorStop();

void motorSetMaxSpeed(float speed_steps_per_sec);
void motorSetAccel(float accel_steps_per_sec2);
void motorUpdateSpeedFromPot(int potRaw);

long motorGetCurrentPosition();
void motorSetCurrentPosition(long pos);

// Ручное движение (для MANUAL_MOVE / калибровки)
void motorManualUp();
void motorManualDown();
void motorManualStop();
