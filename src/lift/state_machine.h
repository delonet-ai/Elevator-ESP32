#pragma once
#include <Arduino.h>
#include "protocol.h"

// Конечный автомат лифта.
//
// smTick()      — периодическая логика, TICK_INTERVAL_MS.
// smFastPoll()  — защиты, каждый проход loop(). Концевик и программные
//                 пределы обрабатываются именно здесь: на тике в 20 мс
//                 кабина на крейсерской скорости успевает проехать
//                 несколько десятков шагов после срабатывания.

void smInit();
void smTick();
void smFastPoll();

// --- Команды ---
void smCommandMoveToFloor(uint8_t floor);
void smCommandStop();          // работает в любом состоянии
void smCommandStartCalib();
void smCommandCalibDownHold(); // калибровка: удержание «вниз»
void smCommandCalibDownSave();
void smCommandManualUpHold();   // ручное движение: удержание «вверх»
void smCommandManualDownHold(); // ручное движение: удержание «вниз»
void smCommandManualStop();
void smCommandStartHoming();
void smCommandClearError();
void smForceNeedCalib();

// --- Статус ---
LiftState smGetState();
uint8_t   smGetCurrentFloor();
uint8_t   smGetTargetFloor();
int8_t    smGetDirection();
uint8_t   smGetError();
long      smGetPosition();
bool      smPositionKnown();
void      smPrintStatus(Stream &out);
