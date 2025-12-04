#include "serial_interface.h"
#include "state_machine.h"

static String inputLine;

void serialInit() {
  inputLine.reserve(64);
  Serial.println("[SERIAL] Ready. Commands: F1/F2/F3, STOP, CALIB, CALIB_DOWN_START, CALIB_DOWN_SAVE, STATUS, CLEAR, MAN_UP, MAN_DOWN, MAN_STOP");
}

static void handleCommand(const String &cmd) {
  if (cmd == "F1") {
    smCommandMoveToFloor(1);
  } else if (cmd == "F2") {
    smCommandMoveToFloor(2);
  } else if (cmd == "F3") {
    smCommandMoveToFloor(3);
  } else if (cmd == "STOP") {
    smCommandStop();
  } else if (cmd == "CALIB") {
    smCommandStartCalib();
  } else if (cmd == "CALIB_DOWN_START") {
    smCommandCalibDownStart();
  } else if (cmd == "CALIB_DOWN_SAVE") {
    smCommandCalibDownSave();
  } else if (cmd == "STATUS") {
    smPrintStatus(Serial);
  } else if (cmd == "CLEAR") {
    smCommandClearError();
  } else if (cmd == "MAN_UP") {
    smCommandManualUpStart();
  } else if (cmd == "MAN_DOWN") {
    smCommandManualDownStart();
  } else if (cmd == "MAN_STOP") {
    smCommandManualStop();
  } else {
    Serial.print("[SERIAL] Unknown command: ");
    Serial.println(cmd);
  }
}

void serialUpdate() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (inputLine.length() > 0) {
        inputLine.trim();
        handleCommand(inputLine);
        inputLine = "";
      }
    } else {
      inputLine += c;
      if (inputLine.length() > 63) {
        inputLine = "";
      }
    }
  }
}
