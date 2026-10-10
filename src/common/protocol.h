#pragma once
//
// Протокол обмена между базой (лифт) и пультом.
// Файл общий для обеих прошивок — правится ТОЛЬКО здесь.
//
#include <stdint.h>

// Версия протокола. Повышается при любом несовместимом изменении структур.
static const uint8_t PROTO_VERSION = 2;

// Сигнатура пакета: отсекает чужой ESP-NOW трафик и мусор, совпавший по длине.
static const uint8_t PROTO_MAGIC = 0xE1;

// Начальный канал поиска пульта. База использует канал своей сети/AP,
// пульт автоматически ищет её на разрешённых каналах.
static const uint8_t ESPNOW_CHANNEL = 1;

// Период отправки статуса база -> пульт.
static const uint32_t STATUS_PERIOD_MS = 200;
// Remote -> base presence diagnostics, independent of manual hold protection.
static const uint32_t REMOTE_HEARTBEAT_MS = 1000;
static const uint32_t REMOTE_PRESENCE_TIMEOUT_MS = 3000;

// --- Команды: пульт -> база -------------------------------------------------

enum CommandType : uint8_t {
  CMD_NONE             = 0,
  CMD_CALL_FLOOR       = 1,  // arg = 1..3
  CMD_STOP             = 2,  // экстренный стоп, работает в любом состоянии
  CMD_CALIB            = 3,  // начать калибровку (хоминг вверх)
  CMD_CALIB_DOWN_START = 4,  // калибровка: поехали вниз
  CMD_CALIB_DOWN_SAVE  = 5,  // калибровка: зафиксировать низ
  CMD_MANUAL_UP        = 6,  // ручное движение вверх (удержание)
  CMD_MANUAL_DOWN      = 7,  // ручное движение вниз (удержание)
  CMD_MANUAL_STOP      = 8,  // кнопка отпущена
  CMD_CLEAR_ERROR      = 9,  // сброс ошибки
  CMD_HOME             = 10, // хоминг к верхнему концевику
  CMD_DISCOVER         = 11  // широковещательный запрос "где база?"
};

// --- Состояния автомата -----------------------------------------------------
// Новые состояния добавляются В КОНЕЦ, чтобы не ломать числовые значения.

enum LiftState : uint8_t {
  STATE_BOOT               = 0,
  STATE_NEED_CALIB         = 1,
  STATE_CALIB_HOMING_UP    = 2,
  STATE_CALIB_MOVING_DOWN  = 3,
  STATE_IDLE               = 4,
  STATE_MOVING             = 5,
  STATE_MANUAL_MOVE        = 6,
  STATE_ERROR              = 7,
  STATE_NEED_HOMING        = 8,  // калибровка есть, но позиция кабины неизвестна
  STATE_HOMING             = 9   // едем вверх к концевику, чтобы её восстановить
};

// --- Коды ошибок ------------------------------------------------------------

enum LiftError : uint8_t {
  ERR_NONE             = 0,
  ERR_MOTION_TIMEOUT   = 1,  // не доехали за отведённое время
  ERR_TOP_LIMIT        = 2,  // концевик сработал там, где не ждали
  ERR_CALIB_TIMEOUT    = 3,  // калибровка не уложилась в таймаут
  ERR_TRAVEL_TOO_SHORT = 4,  // измеренный ход меньше минимально допустимого
  ERR_SOFT_LIMIT       = 5,  // упёрлись в программный предел хода
  ERR_NO_HOME          = 6,  // операция требует известной позиции кабины
  ERR_STORAGE          = 7   // не удалось сохранить/сбросить калибровку в NVS
};

// --- Пакеты -----------------------------------------------------------------
// packed: обе платы — ESP32, но фиксируем раскладку явно, чтобы размер
// не поехал от смены компилятора или флагов.

struct __attribute__((packed)) RemoteCommand {
  uint8_t  magic;    // PROTO_MAGIC
  uint8_t  version;  // PROTO_VERSION
  uint8_t  type;     // CommandType
  uint8_t  arg;      // этаж (1..3) или 0
  uint16_t seq;      // счётчик отправок, для диагностики потерь
};

struct __attribute__((packed)) LiftStatus {
  uint8_t  magic;         // PROTO_MAGIC
  uint8_t  version;       // PROTO_VERSION
  uint8_t  state;         // LiftState
  uint8_t  currentFloor;  // 1..3, 0 = неизвестно
  uint8_t  targetFloor;   // 1..3, 0 = нет цели
  int8_t   direction;     // +1 вверх, -1 вниз, 0 стоим
  uint8_t  error;         // LiftError
  uint8_t  speedPercent;  // текущий предел скорости, % от максимума
  uint8_t  needCalib;     // 1 = требуется калибровка
  uint32_t uptimeMs;
};

// Проверка заголовка. Длину проверяет вызывающая сторона.
inline bool protoHeaderValid(const void *packet) {
  const uint8_t *p = (const uint8_t *)packet;
  return p[0] == PROTO_MAGIC && p[1] == PROTO_VERSION;
}
