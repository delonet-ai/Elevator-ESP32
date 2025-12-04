#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_mac.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ------------------- OLED -------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET   -1
#define OLED_ADDR    0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ------------------- Пины кнопок -------------------
const uint8_t BTN_DOWN = 13;
const uint8_t BTN_UP   = 14;
const uint8_t BTN_F1   = 27;
const uint8_t BTN_F2   = 26;
const uint8_t BTN_F3   = 32;

// ------------------- Пины светодиодов -------------------
const uint8_t LED_DOWN = 19;
const uint8_t LED_UP   = 18;
const uint8_t LED_F1   = 5;
const uint8_t LED_F2   = 4;
const uint8_t LED_F3   = 33;

// ------------------- Протокол (должен совпадать с базой) -------------------

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

enum LiftState : uint8_t {
  STATE_BOOT = 0,
  STATE_NEED_CALIB,
  STATE_CALIB_HOMING_UP,
  STATE_CALIB_MOVING_DOWN,
  STATE_IDLE,
  STATE_MOVING,
  STATE_MANUAL_MOVE,
  STATE_ERROR
};

struct RemoteCommand {
  uint8_t  type;  // CommandType
  uint8_t  arg;   // этаж (1..3) или 0
  uint16_t seq;   // счётчик
};

struct LiftStatus {
  uint8_t state;
  uint8_t currentFloor;
  uint8_t targetFloor;
  int8_t  direction;    // 1=вверх, -1=вниз, 0=стоит
  uint8_t error;
  uint8_t speedPercent;
  uint8_t needCalib;
  uint32_t uptimeMs;
};

// ------------------- ESP-NOW -------------------

// MAC базы (Лифт ESP32). ПОСТАВЬ СВОЙ, если отличается!
uint8_t BASE_MAC[6] = { 0x30, 0xAE, 0xA4, 0x21, 0x33, 0x28 };

// Глобальные данные
static uint16_t g_cmdSeq = 0;

// Последний статус от лифта
static LiftStatus g_status;
static bool       g_hasStatus = false;
static unsigned long g_lastStatusMs = 0;

// Дебаунс / состояние кнопок
bool prevDown = false;
bool prevUp   = false;
bool prevF1   = false;
bool prevF2   = false;
bool prevF3   = false;

// ------------------- Вспомогательные функции -------------------

const char* stateToText(uint8_t st) {
  switch (st) {
    case STATE_BOOT:             return "BOOT";
    case STATE_NEED_CALIB:       return "NEED CAL";
    case STATE_CALIB_HOMING_UP:  return "CAL UP";
    case STATE_CALIB_MOVING_DOWN:return "CAL DOWN";
    case STATE_IDLE:             return "IDLE";
    case STATE_MOVING:           return "MOVING";
    case STATE_MANUAL_MOVE:      return "MANUAL";
    case STATE_ERROR:            return "ERROR";
    default:                     return "?";
  }
}

char directionToChar(int8_t dir) {
  if (dir > 0)  return '^';
  if (dir < 0)  return 'v';
  return '-';
}

void sendCommand(uint8_t type, uint8_t arg) {
  RemoteCommand cmd;
  cmd.type = type;
  cmd.arg  = arg;
  cmd.seq  = ++g_cmdSeq;

  esp_err_t res = esp_now_send(BASE_MAC, (uint8_t*)&cmd, sizeof(cmd));
  Serial.print(F("[REMOTE] Send cmd type="));
  Serial.print(type);
  Serial.print(F(" arg="));
  Serial.print(arg);
  Serial.print(F(" seq="));
  Serial.print(cmd.seq);
  Serial.print(F(" => "));
  Serial.println(res == ESP_OK ? F("OK") : F("ERR"));
}

void updateLeds() {
  // Сначала выключим всё
  digitalWrite(LED_F1, LOW);
  digitalWrite(LED_F2, LOW);
  digitalWrite(LED_F3, LOW);
  digitalWrite(LED_UP, LOW);
  digitalWrite(LED_DOWN, LOW);

  if (!g_hasStatus) {
    // Нет статуса — пусть всё будет погашено
    return;
  }

  unsigned long now = millis();
  bool blink = ((now / 500) % 2) == 0; // 500 мс период мигания

  uint8_t st = g_status.state;

  bool isCalib =
    (st == STATE_NEED_CALIB) ||
    (st == STATE_CALIB_HOMING_UP) ||
    (st == STATE_CALIB_MOVING_DOWN);

  // ================= КАЛИБРОВКА =================
  if (isCalib) {
    // В калибровке: UP и DOWN всегда горят как "основные"
    digitalWrite(LED_UP, HIGH);
    digitalWrite(LED_DOWN, HIGH);

    // 1) Когда нужна калибровка / едем вверх к концевику:
    //    мигает кнопка UP (подсказка "нажми вверх" или "идёт вверх")
    if (st == STATE_NEED_CALIB || st == STATE_CALIB_HOMING_UP) {
      if (blink) {
        digitalWrite(LED_UP, HIGH);
      } else {
        digitalWrite(LED_UP, LOW);
      }
    }

    // 2) Когда едем вниз и нужно поймать нижнюю точку:
    //    мигает кнопка 1-го этажа (F1)
    if (st == STATE_CALIB_MOVING_DOWN) {
      // UP просто горит постоянно
      digitalWrite(LED_UP, HIGH);
      if (blink) {
        digitalWrite(LED_F1, HIGH);
      } else {
        digitalWrite(LED_F1, LOW);
      }
    }

    return;  // в калибровке этажи по обычной логике не отображаем
  }

  // ================= НОРМАЛЬНЫЙ РЕЖИМ =================

  // Кнопки UP и DOWN всегда горят
  digitalWrite(LED_UP, HIGH);
  digitalWrite(LED_DOWN, HIGH);

  uint8_t curFloor = g_status.currentFloor;
  uint8_t tgtFloor = g_status.targetFloor;

  bool isMoving =
    (st == STATE_MOVING) ||
    (st == STATE_MANUAL_MOVE);

  // 1) Стоим (IDLE) на этаже: кнопка этого этажа мигает
  if (!isMoving && curFloor >= 1 && curFloor <= 3) {
    if (blink) {
      switch (curFloor) {
        case 1: digitalWrite(LED_F1, HIGH); break;
        case 2: digitalWrite(LED_F2, HIGH); break;
        case 3: digitalWrite(LED_F3, HIGH); break;
      }
    }
  }

  // 2) Едем к целевому этажу: кнопка целевого этажа горит постоянно
  if (isMoving && tgtFloor >= 1 && tgtFloor <= 3) {
    switch (tgtFloor) {
      case 1: digitalWrite(LED_F1, HIGH); break;
      case 2: digitalWrite(LED_F2, HIGH); break;
      case 3: digitalWrite(LED_F3, HIGH); break;
    }
  }
}

void updateDisplay() {
  display.clearDisplay();

  if (!g_hasStatus) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println(F("NO STATUS"));
    display.setCursor(0, 10);
    display.println(F("Check power/ESP"));
    display.display();
    return;
  }

  uint8_t  st        = g_status.state;
  uint8_t  curFloor  = g_status.currentFloor;
  uint8_t  tgtFloor  = g_status.targetFloor;
  int8_t   dir       = g_status.direction;
  bool     needCal   = g_status.needCalib != 0;
  bool     hasError  = g_status.error != 0;

  // Разбиваем экран на трети по X
  const int16_t W = 128;
  const int16_t H = 32;

  const int16_t leftW   = W / 3;            // ~42
  const int16_t midW    = W / 3;            // ~42
  const int16_t rightW  = W - leftW - midW; // остаток

  const int16_t leftX   = 0;
  const int16_t midX    = leftX + leftW + 13;
  const int16_t rightX  = midX + midW -13;

  // ---------- ЛЕВАЯ ЧАСТЬ: мини-статус ----------
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Строка 1: этажи
  display.setCursor(leftX, 0);
  display.print(F("F "));
  display.print(curFloor);
  display.print(F(" -> "));
  if (tgtFloor == 0) display.print('-');
  else               display.print(tgtFloor);

  // Строка 2: состояние (укороченный текст, чтобы влезало)
  display.setCursor(leftX, 10);
  display.print(F("St: "));
  display.print(stateToText(st)); // если будет длинно — можно позже сократить

  // Строка 3: CAL/ERR
  display.setCursor(leftX, 20);
  if (needCal) {
    display.print(F("CAL "));
  } else {
    display.print(F("    "));
  }
  if (hasError) {
    display.print(F("ERR"));
  }

  // ---------- СЕРЕДИНА: крупный номер этажа ----------
  display.setTextSize(3);  // крупно
  // Центр средний по X и Y
  int16_t midCenterX = midX + midW / 2;
  int16_t midCenterY = H / 2;

  // Оценка ширины цифры при size=3: примерно 6*3 = 18 px
  int16_t digitX = midCenterX - 9;     // сместим на пол-ширины цифры
  int16_t digitY = midCenterY - 12;    // на пол-высоты (8*3/2 ≈12)

  display.setCursor(digitX, digitY);
  if (curFloor == 0) {
    display.print('-');
  } else {
    display.print(curFloor);
  }

  // ---------- ПРАВАЯ ЧАСТЬ: "лифтовая" анимированная стрелка ----------
  // Блок под стрелку – весь rightW по X, по высоте весь экран
  int16_t arrowCenterX = rightX + rightW / 2;

  // Геометрия стрелки
  int16_t arrowTopY    = 4;
  int16_t arrowBottomY = H - 4; // 28
  int16_t arrowHalfW   = (rightW / 2) - 4; // чуть отступим от краёв

  // Анимационный кадр (0..2)
  uint8_t frame = (millis() / 150) % 3;

  if (dir > 0) {
    // ДВИЖЕНИЕ ВВЕРХ

    // Контур стрелки вверх (пустой, только outline)
    display.drawTriangle(
      arrowCenterX,              arrowTopY,
      arrowCenterX - arrowHalfW, arrowBottomY,
      arrowCenterX + arrowHalfW, arrowBottomY,
      SSD1306_WHITE
    );

    // Внутренняя "полоска", которая двигается вверх
    // Разобьём высоту стрелки на 3 зоны и будем сдвигать засветку
    int16_t zoneH = (arrowBottomY - arrowTopY) / 3;
    int16_t segTop = arrowBottomY - (frame + 1) * zoneH;
    int16_t segBottom = segTop + zoneH - 1;

    // Ограничим сегмент, чтобы не вылез
    if (segTop   < arrowTopY)    segTop   = arrowTopY;
    if (segBottom > arrowBottomY) segBottom = arrowBottomY;

    // Нарисуем "полоску" внутри стрелки — просто прямоугольник по центру,
    // такой, чтобы визуально казалось, что в стрелке бегут огни.
    int16_t segW = arrowHalfW;  // половина ширины
    display.fillRect(
      arrowCenterX - segW / 2, segTop,
      segW,                    segBottom - segTop + 1,
      SSD1306_WHITE
    );

  } else if (dir < 0) {
    // ДВИЖЕНИЕ ВНИЗ

    // Контур стрелки вниз
    display.drawTriangle(
      arrowCenterX,              arrowBottomY,
      arrowCenterX - arrowHalfW, arrowTopY,
      arrowCenterX + arrowHalfW, arrowTopY,
      SSD1306_WHITE
    );

    // Внутренняя бегущая полоска вниз
    int16_t zoneH = (arrowBottomY - arrowTopY) / 3;
    int16_t segTop = arrowTopY + frame * zoneH;
    int16_t segBottom = segTop + zoneH - 1;

    if (segTop   < arrowTopY)    segTop   = arrowTopY;
    if (segBottom > arrowBottomY) segBottom = arrowBottomY;

    int16_t segW = arrowHalfW;
    display.fillRect(
      arrowCenterX - segW / 2, segTop,
      segW,                    segBottom - segTop + 1,
      SSD1306_WHITE
    );

  } else {
    // Лифт стоит: горизонтальная черта в правом блоке
    int16_t yLine = H / 2;
    int16_t lineW = rightW - 8;
    display.fillRect(
      arrowCenterX - lineW / 2, yLine - 2,
      lineW,                    4,
      SSD1306_WHITE
    );
  }

  display.display();
}




// ------------------- ESP-NOW колбэки -------------------

void onDataSentRemote(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  // Можно залогировать, если надо
  Serial.print(F("[REMOTE] Send cb: "));
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? F("OK") : F("FAIL"));
}

void onDataRecvRemote(const esp_now_recv_info *recv_info, const uint8_t *incomingData, int len) {
  Serial.print(F("[REMOTE] Data received, len="));
  Serial.println(len);

  if (len == sizeof(LiftStatus)) {
    memcpy(&g_status, incomingData, sizeof(LiftStatus));
    g_hasStatus = true;
    g_lastStatusMs = millis();

    Serial.print(F("[REMOTE] status: state="));
    Serial.print(g_status.state);
    Serial.print(F(" floor="));
    Serial.print(g_status.currentFloor);
    Serial.print(F(" target="));
    Serial.println(g_status.targetFloor);
  } else {
    Serial.println(F("[REMOTE] Unknown packet size"));
  }
}

// ------------------- SETUP / LOOP -------------------

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("=== LIFT REMOTE ==="));

  // Кнопки
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_UP,   INPUT_PULLUP);
  pinMode(BTN_F1,   INPUT_PULLUP);
  pinMode(BTN_F2,   INPUT_PULLUP);
  pinMode(BTN_F3,   INPUT_PULLUP);

  // Светодиоды
  pinMode(LED_DOWN, OUTPUT);
  pinMode(LED_UP,   OUTPUT);
  pinMode(LED_F1,   OUTPUT);
  pinMode(LED_F2,   OUTPUT);
  pinMode(LED_F3,   OUTPUT);

  digitalWrite(LED_DOWN, LOW);
  digitalWrite(LED_UP,   LOW);
  digitalWrite(LED_F1,   LOW);
  digitalWrite(LED_F2,   LOW);
  digitalWrite(LED_F3,   LOW);

  // OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("[OLED] SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Lift Remote"));
  display.setCursor(0, 10);
  display.println(F("Init..."));
  display.display();

  // ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println(F("[ESP-NOW] init FAIL"));
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("ESP-NOW FAIL"));
    display.display();
    return;
  }

  esp_now_register_send_cb(onDataSentRemote);
  esp_now_register_recv_cb(onDataRecvRemote);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BASE_MAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println(F("[ESP-NOW] add_peer FAIL"));
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(F("Peer FAIL"));
    display.display();
    return;
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(F("Lift Remote"));
  display.setCursor(0, 10);
  display.println(F("Ready"));
  display.display();
}

void loop() {
  // Чтение кнопок
  bool nowDown = (digitalRead(BTN_DOWN) == LOW);
  bool nowUp   = (digitalRead(BTN_UP)   == LOW);
  bool nowF1   = (digitalRead(BTN_F1)   == LOW);
  bool nowF2   = (digitalRead(BTN_F2)   == LOW);
  bool nowF3   = (digitalRead(BTN_F3)   == LOW);

  // DOWN: нажали -> MANUAL_DOWN, отпустили -> MANUAL_STOP
  if (nowDown && !prevDown) {
    sendCommand(CMD_MANUAL_DOWN, 0);
  } else if (!nowDown && prevDown) {
    sendCommand(CMD_MANUAL_STOP, 0);
  }

  // UP: нажали -> MANUAL_UP, отпустили -> MANUAL_STOP
  if (nowUp && !prevUp) {
    sendCommand(CMD_MANUAL_UP, 0);
  } else if (!nowUp && prevUp) {
    sendCommand(CMD_MANUAL_STOP, 0);
  }

  // F1/F2/F3: по нажатию — CMD_CALL_FLOOR
  if (nowF1 && !prevF1) {
    sendCommand(CMD_CALL_FLOOR, 1);
  }
  if (nowF2 && !prevF2) {
    sendCommand(CMD_CALL_FLOOR, 2);
  }
  if (nowF3 && !prevF3) {
    sendCommand(CMD_CALL_FLOOR, 3);
  }

  prevDown = nowDown;
  prevUp   = nowUp;
  prevF1   = nowF1;
  prevF2   = nowF2;
  prevF3   = nowF3;

  // Если давно не было статуса — считаем, что связь потеряна
  if (g_hasStatus && (millis() - g_lastStatusMs > 3000)) {
    g_hasStatus = false;
  }

  updateLeds();
  updateDisplay();

  delay(20);
}
