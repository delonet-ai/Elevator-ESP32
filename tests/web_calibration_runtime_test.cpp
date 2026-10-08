// Runs the actual control-loop integration against fake motor/state interfaces.
#include "../src/lift/web_calibration.cpp"
#include <deque>
#include <assert.h>
#include <stdio.h>

FakeWiFi WiFi;
static unsigned long clockMs = 1000;
static LiftState state = STATE_NEED_CALIB;
static bool running = false, networkLocked = false, stopRequested = false;
static int starts = 0, downs = 0, stops = 0, saves = 0, resets = 0;
static long position = 0;
static std::deque<WebCommand> commands;
unsigned long millis() { return clockMs; }
bool motorIsRunning() { return running; }
LiftState smGetState() { return state; }
long smGetPosition() { return position; }
uint8_t smGetError() { return 0; }
bool webMotionLocked() { return networkLocked || webCalibrationBlocksCommands(); }
void smCommandStartCalib() { assert(!webMotionLocked()); ++starts; running=true; state=STATE_CALIB_HOMING_UP; }
void smCommandCalibDownHold() { assert(!webMotionLocked()); ++downs; running=true; }
void smCommandManualStop() { running=false; }
void smCommandCalibDownSave() { assert(!webMotionLocked()); ++saves; state=STATE_IDLE; }
void smForceNeedCalib() { ++resets; state=STATE_NEED_CALIB; }
void smCommandStop() { webCalibrationExternalStop(); ++stops; running=false; if(state==STATE_CALIB_HOMING_UP)state=STATE_NEED_CALIB; }
void smCommandAbortCalib() { smCommandStop(); if(state==STATE_CALIB_MOVING_DOWN)state=STATE_NEED_CALIB; }
bool dashboardTakeStopRequest() { bool value=stopRequested;stopRequested=false;return value; }
bool dashboardTakeCommand(WebCommand &command) { if(commands.empty())return false;command=commands.front();commands.pop_front();return true; }

void send(WebAction action, uint32_t seq, uint32_t client=42) {
  commands.push_back({action,client,webCalibrationGeneration(),seq,(uint32_t)clockMs});
  webCalibrationPoll();
}

int main() {
  networkLocked=true;
  send(WebAction::Start,1);
  assert(starts==0);
  networkLocked=false;
  send(WebAction::Start,1);
  assert(starts==1 && running && webCalibrationBlocksCommands());
  send(WebAction::Start,2,99);
  assert(starts==1 && webCalibrationOwner()==42);
  const uint32_t oldGeneration=webCalibrationGeneration();
  clockMs+=450;
  send(WebAction::Heartbeat,2); // timer checked before the queued heartbeat
  assert(!running && !webCalibrationOwner() && stops==1);
  commands.push_back({WebAction::Start,42,oldGeneration,3,(uint32_t)clockMs});
  webCalibrationPoll();
  assert(starts==1);
  send(WebAction::Start,1);
  assert(starts==2);
  // Simulate top switch, then repeat / reorder down and release packets.
  state=STATE_CALIB_MOVING_DOWN;running=false;position=-1000;
  clockMs+=100;
  send(WebAction::Down,2);
  assert(downs==1 && running);
  send(WebAction::Pause,4);
  assert(!running);
  send(WebAction::Down,3);
  assert(downs==1 && !running);
  send(WebAction::Save,5,99);
  assert(saves==0);
  send(WebAction::Save,5);
  assert(saves==1 && !webCalibrationOwner());
  send(WebAction::Reset,1);
  assert(resets==1);
  send(WebAction::Start,1);
  // A priority STOP invalidates all queued movement, even a full queue.
  for(int i=2;i<6;++i)commands.push_back({WebAction::Down,42,webCalibrationGeneration(),(uint32_t)i,(uint32_t)clockMs});
  stopRequested=true;
  webCalibrationPoll();
  webCalibrationPoll();
  assert(!running && !webCalibrationOwner() && downs==1);
  send(WebAction::Start,1);
  WiFi.value=0;
  state=STATE_CALIB_MOVING_DOWN;
  webCalibrationPoll();
  assert(!running && !webCalibrationOwner() && state==STATE_NEED_CALIB);
  WiFi.value=WL_CONNECTED;
  send(WebAction::Start,1);
  smCommandStop(); // external Serial / remote stop revokes ownership
  assert(!webCalibrationOwner());
  puts("Web calibration runtime: PASS");
}
