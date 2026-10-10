#pragma once
#include <Arduino.h>
#include "protocol.h"

// OLED 128x32, три зоны: слева текстовый статус, по центру крупный номер
// этажа, справа анимированная стрелка направления.

bool uiInit();
void uiRender(bool hasLink, const LiftStatus &status);
void uiShowMessage(const char *line1, const char *line2);
