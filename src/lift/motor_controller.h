#pragma once
#include <Arduino.h>

// Обёртка над FastAccelStepper.
//
// Шаги генерирует периферия ESP32 (RMT/MCPWM), а не loop(), поэтому
// логи и работа Wi-Fi больше не крадут шаги и не сбивают позицию.
// Здесь же живут разгон/торможение и расчёт тормозного пути.

bool motorInit();

// Предел скорости автоматических поездок (шагов/сек).
void motorSetSpeedLimit(float stepsPerSec);
void motorSetAccel(float stepsPerSec2);

// Поездка в заданную позицию с автоматическим торможением.
void motorMoveTo(long position);

// Непрерывное движение до команды стоп (ручной режим, хоминг, калибровка).
void motorRunUp(float stepsPerSec);
void motorRunDown(float stepsPerSec);

// Остановка с торможением — штатное завершение движения.
void motorStopSmooth();
// Мгновенная остановка с ожиданием фактического останова.
// Нужна перед переустановкой или чтением позиции: forceStop() опустошает
// очередь шагов, но уже поставленные в неё импульсы ещё дойдут до драйвера,
// и позиция, прочитанная сразу после вызова, будет неверной.
void motorStopHardAndWait();

// Мгновенная остановка без торможения — защиты и экстренный стоп.
// Шаги могут быть потеряны, поэтому после неё позиция считается точной
// только если её сразу переустановили (например, по концевику).
void motorStopHard();

long motorGetPosition();
void motorSetPosition(long position);

bool motorIsRunning();
// +1 вверх, -1 вниз, 0 стоим.
int  motorGetDirection();
