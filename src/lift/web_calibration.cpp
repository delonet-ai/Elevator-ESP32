#include "web_calibration.h"
#include "web_dashboard.h"
#include "web_config.h"
#include "state_machine.h"
#include "motor_controller.h"
#include "config.h"
#include <WiFi.h>

namespace {
WebControlLease lease;
bool dispatching = false;
bool homingSession = false;
// UI result: 0 idle, 1 started, 2 paused, 3 saved, 4 reset,
// 5 lost browser/network, 6 rejected, 7 external stop, 8 controller error,
// 9 homing completed, 10 error cleared.
uint8_t result = 0;

bool stoppedForStart() {
  LiftState s = smGetState();
  return !motorIsRunning() && (s == STATE_NEED_CALIB || s == STATE_IDLE ||
    s == STATE_NEED_HOMING || s == STATE_ERROR || s == STATE_CALIB_MOVING_DOWN);
}
void stopAndRevoke(uint8_t why) {
  const bool wasCalibration = lease.active() &&
    (smGetState() == STATE_CALIB_HOMING_UP || smGetState() == STATE_CALIB_MOVING_DOWN);
  lease.revoke();
  dispatching = true;
  if (wasCalibration) smCommandAbortCalib();
  else smCommandStop();
  dispatching = false;
  result = why;
}
}

bool webCalibrationBlocksCommands() { return lease.active() && !dispatching; }
uint32_t webCalibrationOwner() { return lease.owner; }
uint32_t webCalibrationGeneration() { return lease.generation; }
uint8_t webCalibrationResult() { return result; }
void webCalibrationExternalStop() {
  if (!dispatching && lease.active()) { lease.revoke(); result = 7; }
}

void webCalibrationPoll() {
  // Independent stop mailbox cannot be crowded out by heartbeats.
  if (dashboardTakeStopRequest()) { stopAndRevoke(7); return; }
  const uint32_t now = millis();
  if (lease.active() && (lease.expired(now) || WiFi.status() != WL_CONNECTED)) {
    stopAndRevoke(5);
    return;
  }
  if (lease.active() && homingSession && smGetState() == STATE_IDLE && !motorIsRunning()) {
    lease.revoke(); result = 9; return;
  }
  if (lease.active() && smGetState() != STATE_CALIB_HOMING_UP && smGetState() != STATE_CALIB_MOVING_DOWN &&
      !(homingSession && smGetState() == STATE_HOMING)) {
    stopAndRevoke(8);
    return;
  }
  WebCommand cmd;
  // Bounded work each loop; queue depth is deliberately small.
  for (uint8_t i = 0; i < 4 && dashboardTakeCommand(cmd); ++i) {
    const uint32_t at = millis();
    if (!lease.fresh(cmd, at) || WiFi.status() != WL_CONNECTED) continue;
    if (cmd.action == WebAction::Clear) {
      if (webMotionLocked() || motorIsRunning() || smGetState() != STATE_ERROR) { result = 6; continue; }
      smCommandClearError(); lease.revoke(); result = smGetError() ? 8 : 10; continue;
    }
    if (cmd.action == WebAction::Start || cmd.action == WebAction::Home) {
      const bool home = cmd.action == WebAction::Home;
      if (webMotionLocked() || !stoppedForStart() || (home && smGetState() != STATE_NEED_HOMING) ||
          !lease.claim(cmd, at)) { result = 6; continue; }
      homingSession = home;
      dispatching = true;
      if (home) smCommandStartHoming();
      else smCommandStartCalib();
      dispatching = false;
      result = 1;
      continue;
    }
    if (cmd.action == WebAction::Reset) {
      if ((lease.active() && !lease.accept(cmd, at)) ||
          (!lease.active() && webMotionLocked()) || !stoppedForStart()) { result = 6; continue; }
      dispatching = true;
      smForceNeedCalib();
      dispatching = false;
      lease.revoke();
      result = smGetError() ? 8 : 4;
      continue;
    }
    if (!lease.accept(cmd, at)) continue;
    dispatching = true;
    switch (cmd.action) {
      case WebAction::Heartbeat: break;
      case WebAction::Down:
        if (smGetState() == STATE_CALIB_MOVING_DOWN) smCommandCalibDownHold();
        break;
      case WebAction::Pause:
        if (smGetState() == STATE_CALIB_MOVING_DOWN) { smCommandManualStop(); result = 2; }
        break;
      case WebAction::Save:
        if (smGetState() == STATE_CALIB_MOVING_DOWN && !motorIsRunning() &&
            smGetPosition() <= -(TOP_MARGIN_STEPS + MIN_TRAVEL_STEPS)) {
          smCommandCalibDownSave();
          lease.revoke();
          result = smGetError() ? 8 : 3;
        } else result = 6;
        break;
      default: break;
    }
    dispatching = false;
  }
}
