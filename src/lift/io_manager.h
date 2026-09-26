#pragma once
#include <Arduino.h>

// Опрос входов базы с программным подавлением дребезга.
// ioUpdate() обязан вызываться каждый проход loop(): на нём построены
// и защиты, и распознавание длинного нажатия.

void ioInit();
void ioUpdate();

// Верхний концевик (после дебаунса).
bool ioTopSwitchActive();

// Кнопка сброса калибровки (после дебаунса).
bool ioCalibButtonPressed();

// Кнопка сброса удерживается дольше CALIB_LONG_PRESS_MS.
// Возвращает true один раз за нажатие.
bool ioCalibLongPressTriggered();

// Сырое значение потенциометра, 0..4095 (усреднённое).
int ioReadPotRaw();

// Предел скорости по потенциометру, 0..100 %.
uint8_t ioSpeedPercent();
