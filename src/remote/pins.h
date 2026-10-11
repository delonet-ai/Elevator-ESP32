#pragma once
//
// Распиновка пульта. Единственный источник правды.
//
#include <stdint.h>

// --- Кнопки (замыкают на GND, включён внутренний подтягивающий резистор) ---
static const uint8_t PIN_BTN_DOWN = 13;
static const uint8_t PIN_BTN_UP   = 14;
static const uint8_t PIN_BTN_F1   = 27;
static const uint8_t PIN_BTN_F2   = 26;
static const uint8_t PIN_BTN_F3   = 32;

// --- Подсветка кнопок ---
static const uint8_t PIN_LED_DOWN = 19;
static const uint8_t PIN_LED_UP   = 18;
static const uint8_t PIN_LED_F1   = 5;
static const uint8_t PIN_LED_F2   = 4;
static const uint8_t PIN_LED_F3   = 33;

// --- OLED SSD1306 128x32, I2C ---
static const uint8_t PIN_OLED_SDA = 21;
static const uint8_t PIN_OLED_SCL = 22;
