#pragma once
#include <Arduino.h>

void ioInit();
void ioUpdate();

// Входы
bool ioReadTopSwitch();
bool ioReadCalibButton();
bool ioReadStopButton();
int  ioReadPotSpeed();
