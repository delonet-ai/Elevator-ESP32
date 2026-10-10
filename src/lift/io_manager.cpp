#include "io_manager.h"
#include "pins.h"
#include "config.h"
#include "log.h"

namespace {

// Дебаунсер уровня: значение меняется только если продержалось stableMs.
struct Debounced {
  bool          state     = false;
  bool          candidate = false;
  unsigned long changedAt = 0;

  void update(bool raw, unsigned long now, unsigned long stableMs) {
    if (raw != candidate) {
      candidate = raw;
      changedAt = now;
      return;
    }
    if (raw != state && (now - changedAt) >= stableMs) {
      state = raw;
    }
  }
};

Debounced g_topSwitch;

#ifdef BENCH_COMMANDS
bool g_benchTopOverride = false;
bool g_benchTopValue    = false;
#endif
Debounced g_calibButton;

bool          g_calibPressPrev     = false;
unsigned long g_calibPressStart    = 0;
bool          g_calibLongFired     = false;
bool          g_calibLongPending   = false;

// Потенциометр шумит; сглаживаем скользящим средним, иначе предел скорости
// дёргается на десятки процентов между тиками.
const int POT_SAMPLES = 8;
int  g_potSamples[POT_SAMPLES];
int  g_potIndex = 0;
long g_potSum   = 0;

}  // namespace

void ioInit() {
  pinMode(PIN_TOP_SWITCH,  INPUT_PULLUP);
  pinMode(PIN_CALIB_RESET, INPUT_PULLUP);
  // Потенциометр на ADC1 (GPIO34) — pinMode не требуется.

  int initial = analogRead(PIN_POT_SPEED);
  for (int i = 0; i < POT_SAMPLES; i++) {
    g_potSamples[i] = initial;
  }
  g_potSum = (long)initial * POT_SAMPLES;

  unsigned long now = millis();
  g_topSwitch.state     = (digitalRead(PIN_TOP_SWITCH) == LOW);
  g_topSwitch.candidate = g_topSwitch.state;
  g_topSwitch.changedAt = now;
  g_calibButton.state     = (digitalRead(PIN_CALIB_RESET) == LOW);
  g_calibButton.candidate = g_calibButton.state;
  g_calibButton.changedAt = now;

  LOG_I("[IO] Init: top=%u pot=%u calib=%u",
        PIN_TOP_SWITCH, PIN_POT_SPEED, PIN_CALIB_RESET);
}

void ioUpdate() {
  unsigned long now = millis();

  g_topSwitch.update(digitalRead(PIN_TOP_SWITCH) == LOW, now, SWITCH_DEBOUNCE_MS);
  g_calibButton.update(digitalRead(PIN_CALIB_RESET) == LOW, now, BUTTON_DEBOUNCE_MS);

  // Длинное нажатие кнопки сброса калибровки.
  bool pressed = g_calibButton.state;
  if (pressed && !g_calibPressPrev) {
    g_calibPressStart = now;
    g_calibLongFired  = false;
  }
  if (pressed && !g_calibLongFired && (now - g_calibPressStart) >= CALIB_LONG_PRESS_MS) {
    g_calibLongFired   = true;
    g_calibLongPending = true;
  }
  g_calibPressPrev = pressed;

  // Потенциометр: одна выборка за проход, окно POT_SAMPLES.
  int sample = analogRead(PIN_POT_SPEED);
  g_potSum -= g_potSamples[g_potIndex];
  g_potSamples[g_potIndex] = sample;
  g_potSum += sample;
  g_potIndex = (g_potIndex + 1) % POT_SAMPLES;
}

bool ioTopSwitchActive() {
#ifdef BENCH_COMMANDS
  if (g_benchTopOverride) return g_benchTopValue;
#endif
  return g_topSwitch.state;
}

#ifdef BENCH_COMMANDS
void ioBenchOverrideTopSwitch(bool active) {
  g_benchTopOverride = true;
  g_benchTopValue    = active;
}

void ioBenchClearOverride() {
  g_benchTopOverride = false;
}
#endif

bool ioCalibButtonPressed() {
  return g_calibButton.state;
}

bool ioCalibLongPressTriggered() {
  if (!g_calibLongPending) return false;
  g_calibLongPending = false;
  return true;
}

int ioReadPotRaw() {
  return (int)(g_potSum / POT_SAMPLES);
}

uint8_t ioSpeedPercent() {
  int raw = ioReadPotRaw();
  if (raw < 0) raw = 0;
  if (raw > 4095) raw = 4095;
  return (uint8_t)((raw * 100L) / 4095);
}
