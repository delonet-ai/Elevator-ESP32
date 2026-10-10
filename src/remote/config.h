#pragma once
#include <Arduino.h>

// --- Дисплей ---
static const uint8_t  OLED_WIDTH   = 128;
static const uint8_t  OLED_HEIGHT  = 32;
static const uint8_t  OLED_ADDR    = 0x3C;
static const int8_t   OLED_RESET   = -1;
// I2C на 400 кГц: на стандартных 100 кГц полная перерисовка 128x32
// занимает около 50 мс и блокирует опрос кнопок.
static const uint32_t OLED_I2C_HZ  = 400000;

// --- Периоды (мс) ---
static const uint32_t DISPLAY_PERIOD_MS   = 150;  // он же шаг анимации стрелки
static const uint32_t LED_PERIOD_MS       = 25;
static const uint32_t BUTTON_DEBOUNCE_MS  = 25;
// Пока кнопка UP/DOWN удержана, команда повторяется: база останавливается
// сама, если повторы пропали. Период должен быть заметно меньше
// MANUAL_HOLD_TIMEOUT_MS на базе (500 мс).
static const uint32_t HOLD_REPEAT_MS      = 150;
// Сколько ждём статус, прежде чем считать связь потерянной.
static const uint32_t LINK_TIMEOUT_MS     = 1500;
// Период широковещательного поиска базы, когда связи нет.
static const uint32_t DISCOVER_PERIOD_MS  = 1000;
// Дублирование команды отпускания: один потерянный пакет не должен
// оставить кабину в движении.
static const uint8_t  STOP_REPEAT_COUNT   = 3;

static const uint32_t BLINK_PERIOD_MS      = 500;
static const uint32_t BLINK_FAST_PERIOD_MS = 150;
