#include "web_config.h"
#include "state_machine.h"
#include "motor_controller.h"
#include "io_manager.h"
#include "comm_lift.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include "web_dashboard.h"
#include "floor_manager.h"
#include "web_calibration.h"
#include "config.h"
#include <Preferences.h>
#include <esp_system.h>

namespace {
WiFiManager manager;
String apName, password;
bool locked = true;
bool serving = false;
bool initialized = false;
unsigned long disconnectedAt = 0;

bool stationary() {
  const LiftState state = smGetState();
  return !motorIsRunning() && state != STATE_MOVING &&
         state != STATE_MANUAL_MOVE && state != STATE_HOMING &&
         state != STATE_CALIB_HOMING_UP && state != STATE_CALIB_MOVING_DOWN;
}

void startPortal() {
  locked = true;
  dashboardStop();
  serving = false;
  // Configuration portal must not expose unauthenticated WiFiManager routes
  // to the household LAN. It is reachable only through the protected AP.
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  manager.startConfigPortal(apName.c_str(), password.c_str());
  webPrintNetwork();
}
}

bool webMotionLocked() { return locked || webCalibrationBlocksCommands(); }

void webPrintNetwork() {
  Serial.printf("[WEB] Setup AP: %s password: %s\n", apName.c_str(), password.c_str());
  Serial.printf("[WEB] Dashboard login: admin / same setup password; IP: %s channel: %d\n",
                WiFi.localIP().toString().c_str(), WiFi.channel());
  if (manager.getConfigPortalActive()) Serial.println("[WEB] Setup: http://192.168.4.1 (motion locked)");
}

void webInit() {
  Preferences prefs;
  if (!prefs.begin("lift-web", false)) {
    Serial.println("[WEB] Cannot open credentials storage; motion remains locked");
    return;
  }
  password = prefs.getString("password", "");
  if (password.length() < 12) {
    char generated[17];
    snprintf(generated, sizeof(generated), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
    password = generated;
    if (!prefs.putString("password", password)) {
      prefs.end();
      Serial.println("[WEB] Cannot save setup password; motion remains locked");
      return;
    }
  }
  prefs.end();
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06lx", (unsigned long)(ESP.getEfuseMac() & 0xffffff));
  apName = String("Lift-Setup-") + suffix;
  if (!dashboardInit(password.c_str())) {
    Serial.println("[WEB] Dashboard initialization failed; motion remains locked");
    return;
  }
  manager.setDebugOutput(false);
  manager.setConfigPortalBlocking(false);
  manager.setConnectTimeout(8);
  manager.setSaveConnectTimeout(8);
  manager.setWiFiAutoReconnect(false);
  manager.setConfigPortalTimeout(0);
  std::vector<const char*> menu = {"wifi", "info"};
  manager.setMenu(menu);
  WiFi.setAutoReconnect(false);
  manager.setAPCallback([](WiFiManager*) { locked = true; });
  // Initial connection may wait up to eight seconds, before any commands
  // are serviced and before ESP-NOW is initialized.
  manager.autoConnect(apName.c_str(), password.c_str());
  WiFi.setAutoReconnect(false);
  initialized = true;
  webPrintNetwork();
}

void webUpdate() {
  if (!initialized) return;
  const unsigned long now = millis();
  static unsigned long lastSnapshot = 0;
  if (now - lastSnapshot >= 100) {
    lastSnapshot = now;
    WebSnapshot snapshot = {};
    snapshot.state = smGetState();
    snapshot.floor = smGetCurrentFloor();
    snapshot.target = smGetTargetFloor();
    snapshot.error = smGetError();
    snapshot.speed = ioSpeedPercent();
    snapshot.channel = WiFi.channel();
    snapshot.known = smPositionKnown();
    snapshot.top = ioTopSwitchActive();
    snapshot.peer = commHasPeer();
    snapshot.stationary = stationary() && !locked;
    snapshot.calibOwner = webCalibrationOwner();
    snapshot.calibGeneration = webCalibrationGeneration();
    snapshot.calibResult = webCalibrationResult();
    snapshot.calibCanStart = !webMotionLocked() && !motorIsRunning() &&
      (stationary() || smGetState() == STATE_CALIB_MOVING_DOWN);
    snapshot.calibCanSave = snapshot.calibOwner && smGetState() == STATE_CALIB_MOVING_DOWN &&
      !motorIsRunning() && smGetPosition() <= -(TOP_MARGIN_STEPS + MIN_TRAVEL_STEPS);
    snapshot.running = motorIsRunning();
    snapshot.position = smGetPosition();
    snapshot.travel = floorGetFullTravelSteps();
    snapshot.uptime = now;
    snapshot.freeHeap = ESP.getFreeHeap();
    snapshot.rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
    snprintf(snapshot.ip, sizeof(snapshot.ip), "%s", WiFi.localIP().toString().c_str());
    dashboardPublish(snapshot);
  }

  uint32_t requestedAt;
  if (dashboardTakeNetworkRequest(requestedAt)) {
    // Recheck in the control loop: the lift may have started since the
    // browser read its snapshot. Never defer a rejected request until idle.
    if (!locked && stationary() && millis() - requestedAt <= 2000) startPortal();
    else dashboardRejectNetworkRequest();
  }
  // Provisioning may block; the dashboard itself runs in the HTTP task.
  if (!stationary()) return;
  if (manager.getConfigPortalActive()) {
    locked = true;
    manager.process();
    return;
  }
  locked = false;
  if (WiFi.status() == WL_CONNECTED) {
    disconnectedAt = 0;
    if (!serving) {
      dashboardStart();
      serving = true;
      webPrintNetwork();
    }
  } else {
    if (disconnectedAt == 0) disconnectedAt = now;
    if (now - disconnectedAt > 15000) {
      disconnectedAt = 0;
      startPortal();
    }
  }
}
