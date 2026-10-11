#include "remote_ui.h"
#include "pins.h"
#include "config.h"
#include "log.h"

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

namespace {

Adafruit_SSD1306 g_display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET);
bool g_ready = false;

// Зоны экрана.
const int16_t LEFT_X   = 0;
const int16_t MID_X    = 56;
const int16_t MID_W    = 28;
const int16_t ARROW_X  = 90;
const int16_t ARROW_W  = OLED_WIDTH - ARROW_X;

// Текст состояния — максимум 8 символов, иначе не влезает в левую зону.
const char *stateText(uint8_t state) {
  switch (state) {
    case STATE_BOOT:              return "BOOT";
    case STATE_NEED_CALIB:        return "NEED CAL";
    case STATE_CALIB_HOMING_UP:   return "CAL UP";
    case STATE_CALIB_MOVING_DOWN: return "CAL DOWN";
    case STATE_IDLE:              return "IDLE";
    case STATE_MOVING:            return "MOVING";
    case STATE_MANUAL_MOVE:       return "MANUAL";
    case STATE_ERROR:             return "ERROR";
    case STATE_NEED_HOMING:       return "NEED HOM";
    case STATE_HOMING:            return "HOMING";
    default:                      return "?";
  }
}

void drawArrow(int8_t direction) {
  const int16_t centerX = ARROW_X + ARROW_W / 2;
  const int16_t topY    = 4;
  const int16_t bottomY = OLED_HEIGHT - 4;
  const int16_t halfW   = ARROW_W / 2 - 4;

  if (direction == 0) {
    // Стоим: толстая горизонтальная черта.
    int16_t lineW = ARROW_W - 8;
    g_display.fillRect(centerX - lineW / 2, OLED_HEIGHT / 2 - 2, lineW, 4,
                       SSD1306_WHITE);
    return;
  }

  const bool up = direction > 0;
  if (up) {
    g_display.drawTriangle(centerX, topY,
                           centerX - halfW, bottomY,
                           centerX + halfW, bottomY, SSD1306_WHITE);
  } else {
    g_display.drawTriangle(centerX, bottomY,
                           centerX - halfW, topY,
                           centerX + halfW, topY, SSD1306_WHITE);
  }

  // Внутри контура «бежит» сегмент — так направление читается с одного
  // взгляда, даже если стрелка мелкая.
  const uint8_t frame = (millis() / DISPLAY_PERIOD_MS) % 3;
  const int16_t zoneH = (bottomY - topY) / 3;
  int16_t segTop = up ? (bottomY - (frame + 1) * zoneH)
                      : (topY + frame * zoneH);
  int16_t segBottom = segTop + zoneH - 1;

  if (segTop < topY)          segTop = topY;
  if (segBottom > bottomY)    segBottom = bottomY;
  if (segBottom < segTop)     return;

  const int16_t segW = halfW;
  g_display.fillRect(centerX - segW / 2, segTop, segW, segBottom - segTop + 1,
                     SSD1306_WHITE);
}

}  // namespace

bool uiInit() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(OLED_I2C_HZ);

  if (!g_display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    LOG_E("[UI] SSD1306 init failed at 0x%02X", OLED_ADDR);
    g_ready = false;
    return false;
  }

  g_ready = true;
  g_display.clearDisplay();
  g_display.setTextColor(SSD1306_WHITE);
  g_display.display();
  return true;
}

void uiShowMessage(const char *line1, const char *line2) {
  if (!g_ready) return;
  g_display.clearDisplay();
  g_display.setTextSize(1);
  g_display.setTextColor(SSD1306_WHITE);
  g_display.setCursor(0, 4);
  g_display.println(line1);
  if (line2 != nullptr) {
    g_display.setCursor(0, 18);
    g_display.println(line2);
  }
  g_display.display();
}

void uiRender(bool hasLink, const LiftStatus &status) {
  if (!g_ready) return;

  if (!hasLink) {
    uiShowMessage("NO LINK", "searching base..");
    return;
  }

  g_display.clearDisplay();
  g_display.setTextColor(SSD1306_WHITE);
  g_display.setTextSize(1);

  // ---- Левая зона: этажи, состояние, флаги ----
  g_display.setCursor(LEFT_X, 0);
  g_display.print('F');
  if (status.currentFloor == 0) g_display.print('-');
  else                          g_display.print(status.currentFloor);
  g_display.print(F(" >"));
  if (status.targetFloor == 0) g_display.print('-');
  else                         g_display.print(status.targetFloor);

  g_display.setCursor(LEFT_X, 11);
  g_display.print(stateText(status.state));

  g_display.setCursor(LEFT_X, 22);
  if (status.error != ERR_NONE) {
    g_display.print(F("ERR "));
    g_display.print(status.error);
  } else if (status.needCalib) {
    g_display.print(F("CALIB"));
  } else {
    g_display.print(status.speedPercent);
    g_display.print('%');
  }

  // ---- Центр: крупный номер этажа ----
  g_display.setTextSize(3);
  // Цифра при size=3 занимает 18x24 px.
  g_display.setCursor(MID_X + MID_W / 2 - 9, OLED_HEIGHT / 2 - 12);
  if (status.currentFloor == 0) g_display.print('-');
  else                          g_display.print(status.currentFloor);

  // ---- Право: стрелка ----
  drawArrow(status.direction);

  g_display.display();
}
