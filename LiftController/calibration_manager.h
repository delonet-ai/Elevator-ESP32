#pragma once
#include <Arduino.h>

// Инициализация модуля калибровки
void calibInit();

// Шаги процесса калибровки
void calibStartHomingUp();   // старт хоминга вверх до концевика
void calibOnTopReached();    // вызов при срабатывании верхнего концевика в режиме калибровки
void calibStartMovingDown(); // старт движения вниз в режиме калибровки
void calibSaveBottom();      // остановка внизу и сохранение калибровки
void motorCalibDownFast();   // теперь вниз ×2
// Периодическое обслуживание (пока можно не использовать)
void calibUpdate();

// Статус калибровки
bool calibHasValidData();  // есть ли валидная калибровка (по нашим данным)

// Сброс калибровки (для долгого нажатия кнопки на базе)
void calibForceReset();    // стереть калибровку и пометить как "нет калибровки"
