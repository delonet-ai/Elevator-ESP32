#include "serial_interface.h"
#include "state_machine.h"
#include "io_manager.h"
#include "floor_manager.h"
#include "motor_controller.h"
#include "log.h"
#include "web_config.h"

namespace {

char   g_line[65];
size_t g_len = 0;

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  F1 F2 F3          - ехать на этаж"));
  Serial.println(F("  UP DOWN           - ручное движение (одиночный импульс удержания)"));
  Serial.println(F("  MSTOP             - остановить ручное движение"));
  Serial.println(F("  STOP              - экстренный стоп"));
  Serial.println(F("  HOME              - хоминг к верхнему концевику"));
  Serial.println(F("  CALIB             - начать калибровку"));
  Serial.println(F("  CDOWN             - калибровка: импульс движения вниз"));
  Serial.println(F("  CSAVE             - калибровка: зафиксировать низ"));
  Serial.println(F("  RESET             - стереть калибровку"));
  Serial.println(F("  CLEAR             - сбросить ошибку"));
  Serial.println(F("  STATUS            - состояние"));
  Serial.println(F("  HELP              - эта справка"));
  Serial.println(F("  NETWORK           - IP and setup access credentials (USB only)"));
}

void handleCommand(const char *cmd) {
#ifdef BENCH_COMMANDS
  // Стендовые команды: только для проверки логики без собранного лифта.
  long arg = 0;
  if (sscanf(cmd, "BCAL %ld", &arg) == 1) {
    smBenchSetCalibrated(arg);
    return;
  }
  if (!strcmp(cmd, "BTOP1")) { ioBenchOverrideTopSwitch(true);  Serial.println(F("bench: top=ON"));  return; }
  if (!strcmp(cmd, "BTOP0")) { ioBenchOverrideTopSwitch(false); Serial.println(F("bench: top=OFF")); return; }
  if (!strcmp(cmd, "BTOPX")) { ioBenchClearOverride();          Serial.println(F("bench: top=real")); return; }
#endif

  if      (!strcmp(cmd, "F1"))     smCommandMoveToFloor(1);
  else if (!strcmp(cmd, "F2"))     smCommandMoveToFloor(2);
  else if (!strcmp(cmd, "F3"))     smCommandMoveToFloor(3);
  else if (!strcmp(cmd, "UP"))     smCommandManualUpHold();
  else if (!strcmp(cmd, "DOWN"))   smCommandManualDownHold();
  else if (!strcmp(cmd, "MSTOP"))  smCommandManualStop();
  else if (!strcmp(cmd, "STOP"))   smCommandStop();
  else if (!strcmp(cmd, "HOME"))   smCommandStartHoming();
  else if (!strcmp(cmd, "CALIB"))  smCommandStartCalib();
  else if (!strcmp(cmd, "CDOWN"))  smCommandCalibDownHold();
  else if (!strcmp(cmd, "CSAVE"))  smCommandCalibDownSave();
  else if (!strcmp(cmd, "RESET"))  smForceNeedCalib();
  else if (!strcmp(cmd, "CLEAR"))  smCommandClearError();
  else if (!strcmp(cmd, "STATUS")) smPrintStatus(Serial);
  else if (!strcmp(cmd, "HELP"))   printHelp();
  else if (!strcmp(cmd, "NETWORK")) webPrintNetwork();
  else Serial.printf("Unknown command: %s\n", cmd);
}

}  // namespace

void serialInit() {
  g_len = 0;
  printHelp();
}

void serialUpdate() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      g_line[g_len] = '\0';
      if (g_len > 0) handleCommand(g_line);
      g_len = 0;
      continue;
    }

    // Переполнение буфера отбрасывает строку целиком, чтобы «хвост»
    // не был выполнен как отдельная команда.
    if (g_len + 1 >= sizeof(g_line)) {
      g_len = 0;
      Serial.println(F("Input too long, discarded"));
      continue;
    }
    g_line[g_len++] = (char)toupper((unsigned char)c);
  }
}
