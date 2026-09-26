//
// Elevator ESP32 — прошивка пульта.
//
// Пульт намеренно «глупый»: он не знает о состояниях лифта и не принимает
// решений. UP/DOWN — это команды удержания, которые повторяются, пока
// кнопка нажата; что делать по ним в текущем состоянии, решает база.
//
// Цикл не блокируется: дисплей перерисовывается по таймеру, а не каждый
// проход, поэтому нажатия не теряются.
//
#include <Arduino.h>

#include "config.h"
#include "log.h"
#include "protocol.h"
#include "remote_buttons.h"
#include "remote_comm.h"
#include "remote_leds.h"
#include "remote_ui.h"

namespace {

unsigned long g_lastDisplayMs = 0;
unsigned long g_lastLedMs     = 0;
unsigned long g_lastHoldUpMs   = 0;
unsigned long g_lastHoldDownMs = 0;

void sendStopRepeated() {
  // Потерянный пакет «кнопка отпущена» оставил бы кабину в движении,
  // поэтому дублируем. База дополнительно страхуется таймаутом удержания.
  for (uint8_t i = 0; i < STOP_REPEAT_COUNT; i++) {
    commSend(CMD_MANUAL_STOP, 0);
  }
}

void handleFloorButton(ButtonId btn, uint8_t floor) {
  if (!buttonJustPressed(btn)) return;

  // Из состояния ошибки любой этаж работает как «сброс ошибки»:
  // отдельной кнопки на пульте нет.
  if (commHasLink() && commStatus().state == STATE_ERROR) {
    commSend(CMD_CLEAR_ERROR, 0);
    return;
  }
  commSend(CMD_CALL_FLOOR, floor);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  LOG_I("[REMOTE] Boot");

  buttonsInit();
  ledsInit();

  if (!uiInit()) {
    LOG_E("[REMOTE] OLED unavailable, continuing without display");
  } else {
    uiShowMessage("Lift Remote", "starting...");
  }

  if (!commInit()) {
    uiShowMessage("ESP-NOW FAIL", "reboot needed");
    LOG_E("[REMOTE] ESP-NOW init failed");
  }

  LOG_I("[REMOTE] Ready");
}

void loop() {
  unsigned long now = millis();

  buttonsUpdate();
  commUpdate();

  // --- UP / DOWN: команды удержания ---
  // Фронты читаем один раз за проход: buttonJustPressed() сбрасывает флаг,
  // и повторный вызов внутри условия его бы «съел».
  const bool upPressed    = buttonJustPressed(BTN_UP);
  const bool upReleased   = buttonJustReleased(BTN_UP);
  const bool downPressed  = buttonJustPressed(BTN_DOWN);
  const bool downReleased = buttonJustReleased(BTN_DOWN);

  if (upPressed) {
    g_lastHoldUpMs = now;
    commSend(CMD_MANUAL_UP, 0);
  } else if (buttonPressed(BTN_UP) && (now - g_lastHoldUpMs >= HOLD_REPEAT_MS)) {
    g_lastHoldUpMs = now;
    commSend(CMD_MANUAL_UP, 0);
  }
  if (upReleased) sendStopRepeated();

  if (downPressed) {
    g_lastHoldDownMs = now;
    commSend(CMD_MANUAL_DOWN, 0);
  } else if (buttonPressed(BTN_DOWN) && (now - g_lastHoldDownMs >= HOLD_REPEAT_MS)) {
    g_lastHoldDownMs = now;
    commSend(CMD_MANUAL_DOWN, 0);
  }
  if (downReleased) sendStopRepeated();

  // --- Этажи ---
  handleFloorButton(BTN_F1, 1);
  handleFloorButton(BTN_F2, 2);
  handleFloorButton(BTN_F3, 3);

  // --- Индикация ---
  if (now - g_lastLedMs >= LED_PERIOD_MS) {
    g_lastLedMs = now;
    ledsUpdate(commHasLink(), commStatus());
  }

  if (now - g_lastDisplayMs >= DISPLAY_PERIOD_MS) {
    g_lastDisplayMs = now;
    uiRender(commHasLink(), commStatus());
  }
}
