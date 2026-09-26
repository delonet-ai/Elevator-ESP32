#pragma once
//
// Единственный источник правды по распиновке базы.
// Раньше пины были продублированы в трёх файлах и расходились с README.
//
#include <stdint.h>

// --- Шаговый драйвер (A4988 / DRV8825 / TMC2208) ---
static const uint8_t PIN_STEP = 18;
static const uint8_t PIN_DIR  = 19;
static const uint8_t PIN_EN   = 21;  // активен низким уровнем

// --- Входы ---
static const uint8_t PIN_TOP_SWITCH  = 32;  // верхний концевик, NO на GND
static const uint8_t PIN_POT_SPEED   = 34;  // потенциометр, only-input пин
static const uint8_t PIN_CALIB_RESET = 33;  // кнопка сброса калибровки, на GND
