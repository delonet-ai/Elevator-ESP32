#pragma once
#include <Arduino.h>

// Опрос пяти кнопок пульта с подавлением дребезга.

enum ButtonId : uint8_t {
  BTN_DOWN = 0,
  BTN_UP,
  BTN_F1,
  BTN_F2,
  BTN_F3,
  BTN_COUNT
};

void buttonsInit();
void buttonsUpdate();

bool buttonPressed(ButtonId id);   // удерживается сейчас
bool buttonJustPressed(ButtonId id);   // фронт нажатия, однократно
bool buttonJustReleased(ButtonId id);  // фронт отпускания, однократно
