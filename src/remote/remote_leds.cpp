#include "remote_leds.h"
#include "pins.h"
#include "config.h"

namespace {

const uint8_t FLOOR_LEDS[3] = {PIN_LED_F1, PIN_LED_F2, PIN_LED_F3};

void allOff() {
  digitalWrite(PIN_LED_UP,   LOW);
  digitalWrite(PIN_LED_DOWN, LOW);
  for (uint8_t i = 0; i < 3; i++) digitalWrite(FLOOR_LEDS[i], LOW);
}

void setFloorLed(uint8_t floor, bool on) {
  if (floor < 1 || floor > 3) return;
  digitalWrite(FLOOR_LEDS[floor - 1], on ? HIGH : LOW);
}

}  // namespace

void ledsInit() {
  pinMode(PIN_LED_UP,   OUTPUT);
  pinMode(PIN_LED_DOWN, OUTPUT);
  for (uint8_t i = 0; i < 3; i++) pinMode(FLOOR_LEDS[i], OUTPUT);
  allOff();
}

void ledsUpdate(bool hasLink, const LiftStatus &status) {
  unsigned long now = millis();
  bool blink     = ((now / BLINK_PERIOD_MS) % 2) == 0;
  bool blinkFast = ((now / BLINK_FAST_PERIOD_MS) % 2) == 0;

  allOff();

  if (!hasLink) {
    // Раньше при потере связи подсветка просто гасла, и пульт выглядел
    // мёртвым. Теперь потеря связи — отдельная, заметная индикация.
    if (blinkFast) {
      digitalWrite(PIN_LED_UP,   HIGH);
      digitalWrite(PIN_LED_DOWN, HIGH);
      for (uint8_t i = 0; i < 3; i++) digitalWrite(FLOOR_LEDS[i], HIGH);
    }
    return;
  }

  if (status.state == STATE_ERROR) {
    if (blinkFast) {
      for (uint8_t i = 0; i < 3; i++) digitalWrite(FLOOR_LEDS[i], HIGH);
    }
    return;
  }

  switch (status.state) {
    case STATE_NEED_CALIB:
    case STATE_CALIB_HOMING_UP:
    case STATE_NEED_HOMING:
    case STATE_HOMING:
      digitalWrite(PIN_LED_UP, blink ? HIGH : LOW);
      return;

    case STATE_CALIB_MOVING_DOWN:
      digitalWrite(PIN_LED_DOWN, HIGH);
      setFloorLed(1, blink);
      return;

    default:
      break;
  }

  // Обычный режим: движение вверх/вниз доступно всегда.
  digitalWrite(PIN_LED_UP,   HIGH);
  digitalWrite(PIN_LED_DOWN, HIGH);

  bool moving = (status.state == STATE_MOVING) || (status.state == STATE_MANUAL_MOVE);

  if (moving && status.targetFloor >= 1 && status.targetFloor <= 3) {
    setFloorLed(status.targetFloor, true);
  } else if (!moving) {
    setFloorLed(status.currentFloor, blink);
  }
}
