#pragma once
#include <Arduino.h>
#include "protocol.h"

// ESP-NOW на стороне пульта.
//
// MAC базы больше не зашит в прошивку: пульт шлёт команды широковещательно,
// пока не получит первый статус, и после этого переключается на unicast.

bool commInit();

// Обслуживание связи: поиск базы, контроль таймаута статуса.
void commUpdate();

void commSend(uint8_t type, uint8_t arg);

bool commHasLink();

// Последний принятый статус. Достоверен, только если commHasLink().
const LiftStatus &commStatus();
