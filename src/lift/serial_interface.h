#pragma once
#include <Arduino.h>

// Текстовый интерфейс отладки в Serial. Дублирует команды пульта,
// чтобы базу можно было проверить без второй платы.

void serialInit();
void serialUpdate();
