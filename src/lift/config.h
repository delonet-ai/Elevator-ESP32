#pragma once
//
// Пределы движения и настройки защит. Изменяемый профиль — в motion_settings.h.
//
#include <Arduino.h>
#include "motion_settings.h"

// --- Абсолютные пределы скорости (шагов/сек) ---
static const float SPEED_MIN = MOTION_AUTO_MIN;
static const float SPEED_MAX = MOTION_AUTO_MAX;

// --- Геометрия шахты (шаги) ---
// Концевик стоит выше 3-го этажа; запас между ними.
static const long TOP_MARGIN_STEPS = 200;
// Меньший ход считаем результатом сорванной калибровки.
static const long MIN_TRAVEL_STEPS = 200;
// Допуск позиционирования на этаже.
static const long POSITION_TOLERANCE = 10;
// Насколько разрешено выехать за пределы [0, fullTravel] в ручном режиме,
// прежде чем сработает программный предел.
static const long SOFT_LIMIT_MARGIN_STEPS = 100;
// Потолок хода в калибровке: страховка на случай неисправного концевика.
static const long CALIB_MAX_TRAVEL_STEPS = 100000;

// --- Таймауты (мс) ---
// Таймаут поездки считается от расстояния: на минимуме регулятора скорости
// полный проезд шахты занимает секунды, и фиксированный порог
// срабатывал бы как ложная ошибка.
static const uint32_t MOTION_TIMEOUT_MIN_MS   = 5000;   // нижняя граница
static const float    MOTION_TIMEOUT_FACTOR   = 1.5f;   // запас к расчётному времени
static const uint32_t HOMING_TIMEOUT_MS       = 30000;  // подъём до концевика
static const uint32_t CALIB_DOWN_TIMEOUT_MS   = 60000;  // спуск в калибровке
// Ручное движение — команда удержания: пульт повторяет её, пока кнопка нажата.
// Если повторы пропали (потеря связи, разряд пульта), база сама встаёт.
static const uint32_t MANUAL_HOLD_TIMEOUT_MS  = 500;
// Абсолютный потолок непрерывного ручного движения.
static const uint32_t MANUAL_MAX_DURATION_MS  = 30000;

// --- Периоды (мс) ---
static const uint32_t TICK_INTERVAL_MS   = 20;    // тик автомата
static const uint32_t CALIB_LONG_PRESS_MS = 3000; // удержание кнопки сброса
static const uint32_t SWITCH_DEBOUNCE_MS = 5;     // дребезг концевика
static const uint32_t BUTTON_DEBOUNCE_MS = 30;    // дребезг кнопки на базе

// Число этажей. Часть логики (деление хода) рассчитана на равные промежутки.
static const uint8_t FLOOR_COUNT = 3;
