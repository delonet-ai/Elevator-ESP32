#pragma once
#include <Arduino.h>

// Состояния лифта
enum LiftState : uint8_t {
    STATE_BOOT,
    STATE_NEED_CALIB,
    STATE_CALIB_HOMING_UP,
    STATE_CALIB_MOVING_DOWN,
    STATE_IDLE,
    STATE_MOVING,
    STATE_MANUAL_MOVE,
    STATE_ERROR
};

// Инициализация и тик автомата
void smInit();
void smTick();

// Команды управления лифтом
void smCommandMoveToFloor(uint8_t floor);  // перейти на этаж 1–3
void smCommandStop();                      // экстренный стоп

// Калибровка
void smCommandStartCalib();        // начать калибровку (поездка вверх к концевику)
void smCommandCalibDownStart();    // в калибровке: начать движение вниз вручную
void smCommandCalibDownSave();     // в калибровке: сохранить нижнюю точку

// Ручной режим
void smCommandManualUpStart();
void smCommandManualDownStart();
void smCommandManualStop();

// Работа с ошибками / статусом
void smCommandClearError();
LiftState smGetState();
void smPrintStatus(Stream &out);

// Дополнительно: доступ к текущему положению/этажам (если нужно)
uint8_t smGetCurrentFloor();
uint8_t smGetTargetFloor();
long    smGetCurrentPosition();

// Специально для кнопки на базе: принудительно перевести лифт в режим NEED_CALIB
void smForceNeedCalib();
