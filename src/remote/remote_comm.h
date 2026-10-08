#pragma once
#include <Arduino.h>
#include "protocol.h"

// ESP-NOW на стороне пульта.
//
// Пульт ищет базу на каналах 1..13 запросами DISCOVER. Команды движения
// разрешены только при свежем статусе и отправляются на найденный MAC.

bool commInit();

// Обслуживание связи: поиск базы, контроль таймаута статуса.
void commUpdate();

void commSend(uint8_t type, uint8_t arg);

bool commHasLink();

// Последний принятый статус. Достоверен, только если commHasLink().
const LiftStatus &commStatus();
