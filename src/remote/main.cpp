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
#include "device_pairing.h"
#include "maintenance.h"
#include <WiFi.h>

namespace {

unsigned long g_lastDisplayMs = 0;
unsigned long g_lastLedMs     = 0;
unsigned long g_lastHoldUpMs   = 0;
unsigned long g_lastHoldDownMs = 0;
bool g_controlsArmed = false;

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

  pairingInit();
  maintenanceInit("");
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
  static char line[64];static size_t length=0;static bool overflow=false;
  while(Serial.available()){
    char c=Serial.read();if(c=='\r')continue;
    if(c=='\n'){if(!overflow){line[length]=0;pairingCommand(line);}length=0;overflow=false;}
    else if(length+1<sizeof(line)&&!overflow)line[length++]=c;else overflow=true;
  }
  bool released=true;for(uint8_t i=0;i<BTN_COUNT;++i)if(buttonPressed(static_cast<ButtonId>(i)))released=false;
  const auto state=commStatus().state;
  bool idle=commHasLink()&&released&&state!=STATE_MOVING&&state!=STATE_MANUAL_MOVE&&state!=STATE_HOMING&&state!=STATE_CALIB_HOMING_UP&&state!=STATE_CALIB_MOVING_DOWN;
  maintenancePoll(idle);
  if(maintenanceActive()){
    g_controlsArmed=false;
    if(now-g_lastDisplayMs>=500){g_lastDisplayMs=now;uiShowMessage("OTA - controls off",WiFi.status()==WL_CONNECTED?WiFi.localIP().toString().c_str():"Connecting Wi-Fi");}
    ledsUpdate(false,commStatus());return;
  }

  if (!commHasLink()) g_controlsArmed = false;
  if (!g_controlsArmed) {
    bool allReleased = true;
    for (uint8_t i = 0; i < BTN_COUNT; ++i) {
      ButtonId id = static_cast<ButtonId>(i);
      if (buttonPressed(id)) allReleased = false;
      buttonJustPressed(id);
      buttonJustReleased(id);
    }
    if (commHasLink() && allReleased) g_controlsArmed = true;
  }

  // --- UP / DOWN: команды удержания ---
  // Фронты читаем один раз за проход: buttonJustPressed() сбрасывает флаг,
  // и повторный вызов внутри условия его бы «съел».
  if (g_controlsArmed) {
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
  }

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
