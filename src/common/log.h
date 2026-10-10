#pragma once
//
// Логирование в Serial с уровнями. Уровень задаётся флагом сборки
// -DLOG_LEVEL=N (см. platformio.ini): 0 — тишина, 3 — всё подряд.
//
// Зачем макросы, а не Serial.print напрямую: при уровне 0 строки
// не попадают в прошивку вообще, а печать в горячих местах (ESP-NOW
// callback, сервис мотора) — это реальные задержки, а не косметика.
//
#include <Arduino.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3

#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_E(fmt, ...) Serial.printf("[ERR ] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_E(fmt, ...) do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_W(fmt, ...) Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_W(fmt, ...) do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_I(fmt, ...) Serial.printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#else
#define LOG_I(fmt, ...) do {} while (0)
#endif
