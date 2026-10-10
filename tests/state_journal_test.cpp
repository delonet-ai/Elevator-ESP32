#include <assert.h>
class Stream { public: template<typename... Args> void printf(const char*, Args...) {} };
#include "../src/lift/state_machine.cpp"
#include "../src/lift/event_journal.cpp"
TestSerial Serial;
static unsigned long clockMs = 100;
static long position = 0;
static bool top = false, running = false, locked = false;
static int direction = 0;
unsigned long millis() { return clockMs; }
bool webMotionLocked() { return locked; }
bool webCalibrationBlocksCommands() { return false; }
void webCalibrationExternalStop() {}
bool ioTopSwitchActive() { return top; }
uint8_t ioSpeedPercent() { return 50; }
bool calibIsValid() { return false; }
void calibMarkTop() { motorStopHardAndWait(); motorSetPosition(0); }
bool calibFinishAtBottom(long) { return false; }
bool calibReset() { return true; }
long floorGetTopSwitchPosition() { return 2200; }
long floorGetPositionForFloor(uint8_t floor) { return (floor - 1) * 1000; }
uint8_t floorGetNearestFloor(long) { return 0; }
long floorGetFullTravelSteps() { return 2000; }
const MotionSettings &motionSettings() { static auto settings = motionDefaults(); return settings; }
void motorSetSpeedLimit(float) {}
void motorMoveTo(long) { running = true; }
void motorRunUp(float) { running = true; direction = 1; }
void motorRunDown(float) { running = true; direction = -1; }
void motorStopHard() { running = false; direction = 0; }
void motorStopHardAndWait() { motorStopHard(); }
void motorStopSmooth() { motorStopHard(); }
void motorSetPosition(long value) { position = value; }
long motorGetPosition() { return position; }
bool motorIsRunning() { return running; }
int motorGetDirection() { return direction; }
int main() {
  smInit();
  assert(smGetState() == STATE_NEED_CALIB);
  top = true;
  smCommandStartCalib(); // Both transitions occur before the next loop iteration.
  auto events = journalSnapshot();
  assert(events.count == 4);
  assert(events.entries[2].value == STATE_CALIB_HOMING_UP);
  assert(events.entries[3].value == STATE_CALIB_MOVING_DOWN);
  assert(events.entries[2].uptime == events.entries[3].uptime);
  assert(!running);
  top = false;
  smCommandCalibDownHold(); assert(running);
  position = -1200;
  smCommandCalibDownSave(); // Failed persistence must still stop and enter error.
  assert(!running && smGetState() == STATE_ERROR && smGetError() == ERR_STORAGE);
  events = journalSnapshot();
  assert(events.entries[4].kind == static_cast<uint8_t>(EventKind::Error));
  assert(events.entries[4].value == ERR_STORAGE && events.entries[4].position == -1200);
  locked = true; running = true;
  smCommandStop(); assert(!running); // Logging does not change unconditional STOP.
  events = journalSnapshot();
  assert(events.entries[events.count-1].kind == static_cast<uint8_t>(EventKind::Stop));
  locked = false; smCommandClearError();
  assert(smGetError() == ERR_NONE && smGetState() == STATE_NEED_CALIB);
}
