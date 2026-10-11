#include "remote_buttons.h"
#include "pins.h"
#include "config.h"

namespace {

const uint8_t PINS[BTN_COUNT] = {
  PIN_BTN_DOWN, PIN_BTN_UP, PIN_BTN_F1, PIN_BTN_F2, PIN_BTN_F3
};

struct ButtonState {
  bool          stable    = false;
  bool          candidate = false;
  unsigned long changedAt = 0;
  bool          rose      = false;
  bool          fell      = false;
};

ButtonState g_buttons[BTN_COUNT];

}  // namespace

void buttonsInit() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    pinMode(PINS[i], INPUT_PULLUP);
    bool level = (digitalRead(PINS[i]) == LOW);
    g_buttons[i].stable    = level;
    g_buttons[i].candidate = level;
    g_buttons[i].changedAt = now;
  }
}

void buttonsUpdate() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    ButtonState &b = g_buttons[i];
    bool raw = (digitalRead(PINS[i]) == LOW);

    if (raw != b.candidate) {
      b.candidate = raw;
      b.changedAt = now;
      continue;
    }
    if (raw != b.stable && (now - b.changedAt) >= BUTTON_DEBOUNCE_MS) {
      b.stable = raw;
      if (raw) b.rose = true;
      else     b.fell = true;
    }
  }
}

bool buttonPressed(ButtonId id) {
  return g_buttons[id].stable;
}

bool buttonJustPressed(ButtonId id) {
  if (!g_buttons[id].rose) return false;
  g_buttons[id].rose = false;
  return true;
}

bool buttonJustReleased(ButtonId id) {
  if (!g_buttons[id].fell) return false;
  g_buttons[id].fell = false;
  return true;
}
