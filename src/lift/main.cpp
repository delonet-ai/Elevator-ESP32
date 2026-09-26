//
// Elevator ESP32 — прошивка базы (лифт).
//
// Структура цикла:
//   ioUpdate()       — опрос входов с дебаунсом;
//   smFastPoll()     — защиты: концевик, пределы хода, дедлайн удержания;
//   commPoll()       — разбор очереди команд от пульта;
//   smTick()         — периодическая логика автомата (TICK_INTERVAL_MS);
//   commSendStatus() — телеметрия на пульт (STATUS_PERIOD_MS).
//
// Шаги мотора генерирует периферия (FastAccelStepper), поэтому задержки
// в цикле больше не приводят к потере позиции.
//
#include <Arduino.h>

#include "config.h"
#include "log.h"
#include "io_manager.h"
#include "motor_controller.h"
#include "floor_manager.h"
#include "calibration_manager.h"
#include "state_machine.h"
#include "comm_lift.h"
#include "serial_interface.h"

static unsigned long g_lastTick = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  LOG_I("[LIFT] Boot");

  ioInit();
  ioUpdate();  // получить достоверное состояние концевика до старта автомата

  if (!motorInit()) {
    LOG_E("[LIFT] Motor init failed, halting");
    // Без мотора двигаться нечем; оставляем Serial живым для диагностики.
  }

  floorInit();
  calibInit();
  smInit();

  if (!commInit()) {
    LOG_E("[LIFT] ESP-NOW init failed, remote will not work");
  }

  serialInit();
  LOG_I("[LIFT] Ready");
}

void loop() {
  ioUpdate();
  smFastPoll();

  // Длинное нажатие кнопки на базе — полный сброс калибровки.
  if (ioCalibLongPressTriggered()) {
    LOG_W("[LIFT] Calibration reset button held, wiping calibration");
    smForceNeedCalib();
  }

  commPoll();
  serialUpdate();

  unsigned long now = millis();
  if (now - g_lastTick >= TICK_INTERVAL_MS) {
    g_lastTick = now;
    smTick();
  }

  commSendStatusIfDue();
}
