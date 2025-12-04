#include "io_manager.h"

// Пины пока поставим заглушками, потом подправим под реальное железо
static const int PIN_TOP_SWITCH   = 32;
static const int PIN_CALIB_BUTTON = 33;
static const int PIN_STOP_BUTTON  = 25;
static const int PIN_POT_SPEED    = 34;

void ioInit() {
  pinMode(PIN_TOP_SWITCH,   INPUT_PULLUP);
  pinMode(PIN_CALIB_BUTTON, INPUT_PULLUP);
  pinMode(PIN_STOP_BUTTON,  INPUT_PULLUP);
  // Потенциометр — просто analogRead
  Serial.println("[IO] Init");
}

void ioUpdate() {
  // сюда можно потом добавить дебаунс кнопок, обработку долгого нажатия и т.п.
}

bool ioReadTopSwitch() {
  return digitalRead(PIN_TOP_SWITCH) == LOW; // активный LOW
}

bool ioReadCalibButton() {
  return digitalRead(PIN_CALIB_BUTTON) == LOW;
}

bool ioReadStopButton() {
  return digitalRead(PIN_STOP_BUTTON) == LOW;
}

int ioReadPotSpeed() {
  return analogRead(PIN_POT_SPEED); // 0..4095
}
