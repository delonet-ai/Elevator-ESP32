#include <Arduino.h>
#include "state_machine.h"
#include "motor_controller.h"
#include "io_manager.h"
#include "serial_interface.h"
#include "floor_manager.h"
#include "calibration_manager.h"
#include "comm_interface.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>

// ======= Протокол лифт <-> пульт (должен быть ИДЕНТИЧЕН на обоих ESP) =======

// Типы команд (пульт -> база)
enum CommandType : uint8_t {
  CMD_NONE             = 0,
  CMD_CALL_FLOOR       = 1, // arg = 1,2,3
  CMD_STOP             = 2,
  CMD_CALIB            = 3,
  CMD_CALIB_DOWN_START = 4,
  CMD_CALIB_DOWN_SAVE  = 5,
  CMD_MANUAL_UP        = 6,
  CMD_MANUAL_DOWN      = 7,
  CMD_MANUAL_STOP      = 8
};

struct RemoteCommand {
  uint8_t  type;   // CommandType
  uint8_t  arg;    // этаж (1..3) или 0
  uint16_t seq;    // счётчик, можно просто печатать
};

struct LiftStatus {
  uint8_t state;
  uint8_t currentFloor;
  uint8_t targetFloor;
  int8_t  direction;
  uint8_t error;
  uint8_t speedPercent;
  uint8_t needCalib;
  uint32_t uptimeMs;
};

// ======= Глобалы ESP-NOW на базе =======

// MAC последнего пульта (от него пришёл пакет)
static uint8_t g_remoteMac[6] = {0};
static bool    g_haveRemoteMac = false;
static bool    g_remotePeerAdded = false;  // ДОБАВЬ ЭТО
// Для периодической отправки статуса (зарезервировано)
static unsigned long g_lastStatusSentMs = 0;
static const unsigned long STATUS_PERIOD_MS = 200; // пока не используем

// Тик автомата
static unsigned long g_lastTick = 0;
static const unsigned long TICK_INTERVAL_MS = 20;

// --- Кнопка полной перекалибровки на базе ---
const uint8_t PIN_CALIB_RESET           = 33;
const unsigned long CALIB_LONG_PRESS_MS = 3000;

bool          g_calibBtnPrev            = false;
bool          g_calibLongPressTriggered = false;
unsigned long g_calibPressStart         = 0;

// ==== Прототипы локальных функций ====
void handleRemoteCommand(const RemoteCommand &cmd);
void onDataSentBase(const wifi_tx_info_t *info, esp_now_send_status_t status);
void onDataRecvBase(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len);
void commInitBase();

// ================== ЛОГИКА ОБРАБОТКИ КОМАНД ОТ ПУЛЬТА ==================

void handleRemoteCommand(const RemoteCommand &cmd) {
  Serial.print(F("[RCV CMD] type="));
  Serial.print(cmd.type);
  Serial.print(F(" arg="));
  Serial.print(cmd.arg);
  Serial.print(F(" seq="));
  Serial.println(cmd.seq);

  LiftState st = smGetState();

  switch (cmd.type) {
    // --- Вызов этажа ---
    case CMD_CALL_FLOOR:
      // В режиме калибровки вызовы этажей игнорируем
      if (st == STATE_NEED_CALIB ||
          st == STATE_CALIB_HOMING_UP ||
          st == STATE_CALIB_MOVING_DOWN) {
        Serial.println(F("[ACT] Ignored: floor call during calibration"));
        break;
      }
      if (cmd.arg >= 1 && cmd.arg <= 3) {
        Serial.print(F("[ACT] Move to floor "));
        Serial.println(cmd.arg);
        smCommandMoveToFloor(cmd.arg);
      } else {
        Serial.println(F("[WARN] Invalid floor in CMD_CALL_FLOOR"));
      }
      break;

    // --- Кнопка ВВЕРХ ---
    case CMD_MANUAL_UP:
      if (st == STATE_NEED_CALIB) {
        // Нужна калибровка → UP запускает калибровку вверх
        Serial.println(F("[ACT] NEED_CALIB: start calibration (UP)"));
        smCommandStartCalib();
      } else if (st == STATE_CALIB_HOMING_UP ||
                 st == STATE_CALIB_MOVING_DOWN) {
        // Во время калибровки игнорируем обычный UP
        Serial.println(F("[ACT] Ignored: UP during calibration"));
      } else {
        // Обычный режим: ручной подъём
        Serial.println(F("[ACT] Manual UP start"));
        smCommandManualUpStart();
      }
      break;

    // --- Кнопка ВНИЗ ---
    case CMD_MANUAL_DOWN:
      if (st == STATE_NEED_CALIB) {
        Serial.println(F("[ACT] Ignored: DOWN in NEED_CALIB (press UP first)"));
      } else if (st == STATE_CALIB_HOMING_UP) {
        Serial.println(F("[ACT] Ignored: DOWN during CALIB_HOMING_UP"));
      } else if (st == STATE_CALIB_MOVING_DOWN) {
        // В калибровке вниз: DOWN запускает движение вниз
        Serial.println(F("[ACT] CALIB_MOVING_DOWN: start auto down"));
        smCommandCalibDownStart();
      } else {
        Serial.println(F("[ACT] Manual DOWN start"));
        smCommandManualDownStart();
      }
      break;

    // --- Отпускание Up/Down (manual stop) ---
    case CMD_MANUAL_STOP:
      if (st == STATE_NEED_CALIB ||
          st == STATE_CALIB_HOMING_UP ||
          st == STATE_CALIB_MOVING_DOWN) {
        Serial.println(F("[ACT] Ignored: Manual STOP during calibration"));
      } else {
        Serial.println(F("[ACT] Manual move STOP"));
        smCommandManualStop();
      }
      break;

    // --- Явные калибровочные команды (резерв) ---
    case CMD_CALIB:
      Serial.println(F("[ACT] Start calibration (explicit CMD_CALIB)"));
      smCommandStartCalib();
      break;

    case CMD_CALIB_DOWN_START:
      Serial.println(F("[ACT] Calib: move down (explicit CMD_CALIB_DOWN_START)"));
      smCommandCalibDownStart();
      break;

    case CMD_CALIB_DOWN_SAVE:
      Serial.println(F("[ACT] Calib: save bottom (explicit CMD_CALIB_DOWN_SAVE)"));
      smCommandCalibDownSave();
      break;

    // --- STOP (экстренный) ---
    case CMD_STOP:
      Serial.println(F("[ACT] EMERGENCY STOP"));
      smCommandStop();
      break;

    case CMD_NONE:
    default:
      Serial.println(F("[ACT] CMD_NONE or unknown cmd"));
      break;
  }

  // Доп. логика: в режиме калибровки вниз, нажатие F1 завершает калибровку
  if (cmd.type == CMD_CALL_FLOOR && cmd.arg == 1 && st == STATE_CALIB_MOVING_DOWN) {
    Serial.println(F("[ACT] CALIB_MOVING_DOWN: F1 pressed, save bottom & finish"));
    smCommandCalibDownSave();
  }
}

// ================== ESP-NOW КОЛБЭКИ ==================

void onDataSentBase(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print(F("[ESP-NOW BASE] Send status: "));
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? F("SUCCESS") : F("FAIL"));
}

void onDataRecvBase(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
  Serial.print(F("[ESP-NOW BASE] Data received, len="));
  Serial.println(len);

  // Запоминаем MAC отправителя (пульта) — потом можно будет слать ему статус
    if (recv_info != nullptr) {
    memcpy(g_remoteMac, recv_info->src_addr, 6);
    g_haveRemoteMac = true;

    Serial.print(F("  From MAC: "));
    for (int i = 0; i < 6; i++) {
      if (i) Serial.print(":");
      Serial.printf("%02X", g_remoteMac[i]);
    }
    Serial.println();

    // Добавляем пульт как peer для отправки статуса
    if (!g_remotePeerAdded) {
      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, g_remoteMac, 6);
      peerInfo.channel = 0;
      peerInfo.encrypt = false;

      esp_err_t r = esp_now_add_peer(&peerInfo);
      if (r == ESP_OK) {
        g_remotePeerAdded = true;
        Serial.println(F("[COMM] Remote peer added for status TX"));
      } else {
        Serial.print(F("[COMM] Failed to add remote peer, err="));
        Serial.println((int)r);
      }
    }
  }


  if (len == sizeof(RemoteCommand)) {
    RemoteCommand cmd;
    memcpy(&cmd, incomingData, sizeof(RemoteCommand));
    handleRemoteCommand(cmd);
  } else {
    Serial.println(F("  Unknown packet size, ignoring"));
  }
}

void sendStatusToRemoteIfNeeded() {
  if (!g_haveRemoteMac || !g_remotePeerAdded) return;  // <--- важно

  unsigned long now = millis();
  if (now - g_lastStatusSentMs < STATUS_PERIOD_MS) return;
  g_lastStatusSentMs = now;

  LiftStatus st;
  LiftState s = smGetState();
  st.state        = (uint8_t)s;
  st.currentFloor = smGetCurrentFloor();
  st.targetFloor  = smGetTargetFloor();

  st.direction = 0;
  if (s == STATE_MOVING) {
    if (st.targetFloor > st.currentFloor) st.direction = 1;
    else if (st.targetFloor < st.currentFloor) st.direction = -1;
  }

  st.error       = (s == STATE_ERROR) ? 1 : 0;
  st.speedPercent= 0;
  st.needCalib   = (s == STATE_NEED_CALIB) ? 1 : 0;
  st.uptimeMs    = now;

  esp_err_t res = esp_now_send(g_remoteMac, (uint8_t*)&st, sizeof(LiftStatus));
  if (res != ESP_OK) {
    Serial.print(F("[COMM] Status send ERR="));
    Serial.println((int)res);
  }
}

// ================== SETUP / LOOP ==================

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println(F("[LIFT] Booting..."));

  pinMode(PIN_CALIB_RESET, INPUT_PULLUP);

  ioInit();
  motorInit();
  floorInit();
  calibInit();
  commInit();   // из comm_interface, если используется
  smInit();
  serialInit();

  Serial.println(F("[LIFT] Setup core done, init ESP-NOW base..."));
  commInitBase();
  Serial.println(F("[LIFT] Setup done."));
}

void loop() {
  // Обновляем вводы (кнопки, концевики, потенциометр и т.д.)
  ioUpdate();

  // --- Обработка длинного нажатия кнопки перекалибровки на базе ---
  bool pressed = (digitalRead(PIN_CALIB_RESET) == LOW);
  unsigned long now = millis();

  if (pressed && !g_calibBtnPrev) {
    // начало нажатия
    g_calibPressStart = now;
    g_calibLongPressTriggered = false;
  }

  if (pressed && !g_calibLongPressTriggered) {
    if (now - g_calibPressStart >= CALIB_LONG_PRESS_MS) {
      g_calibLongPressTriggered = true;
      Serial.println(F("[CALIB] Base button long press: FULL RECALIBRATION"));

      // 1) Сброс калибровки (верх/низ/этажи — через floor_manager/calibration_manager)
      calibForceReset();

      // 2) Перевод автомата в режим NEED_CALIB
      smForceNeedCalib();
    }
  }

  if (!pressed && g_calibBtnPrev) {
    // кнопка отпущена – можно что-то залогировать, если надо
  }

  g_calibBtnPrev = pressed;

  // Обслуживаем движение мотора
  motorService();

  // Обновляем приём команд по Serial (парсер)
  serialUpdate();

  // Обновление связи с пультом (если что-то есть в comm_interface)
  commUpdate();

  // Периодический тик автомата состояний
  unsigned long now2 = millis();
  if (now2 - g_lastTick >= TICK_INTERVAL_MS) {
    g_lastTick = now2;
    smTick();
  }
    // ... после smTick()
  sendStatusToRemoteIfNeeded();
}

// ================== ИНИЦИАЛИЗАЦИЯ ESP-NOW НА БАЗЕ ==================

void commInitBase() {
  Serial.println(F("[COMM] Init ESP-NOW base"));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();  // на всякий случай

  // Для отладки можем вывести MAC базы (eFuse STA MAC)
  uint8_t mac_base[6];
  esp_read_mac(mac_base, ESP_MAC_WIFI_STA);
  Serial.print(F("[COMM] Base MAC (eFuse STA): "));
  for (int i = 0; i < 6; i++) {
    if (i) Serial.print(":");
    Serial.printf("%02X", mac_base[i]);
  }
  Serial.println();

  if (esp_now_init() != ESP_OK) {
    Serial.println(F("[COMM] ESP-NOW init FAILED"));
    return;
  }

  esp_now_register_send_cb(onDataSentBase);
  esp_now_register_recv_cb(onDataRecvBase);

  Serial.println(F("[COMM] ESP-NOW init OK (base)"));
  
}
