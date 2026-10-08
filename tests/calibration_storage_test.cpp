#include "../src/lift/floor_manager.cpp"
#include "../src/lift/calibration_manager.cpp"
#include <assert.h>
#include <stdio.h>
TestSerial Serial;
TestNvs nvs;
static long motorPosition=-1200;
void motorStopHardAndWait() {}
void motorSetPosition(long value) { motorPosition=value; }

int main() {
  floorInit();
  assert(!calibIsValid());
  nvs.failWrite=true;
  assert(!calibFinishAtBottom(-1200));
  assert(!calibIsValid() && motorPosition==-1200);
  nvs.failWrite=false;
  assert(calibFinishAtBottom(-1200));
  assert(calibIsValid() && floorGetFullTravelSteps()==1000 && motorPosition==0);
  nvs.failWrite=true;
  assert(!calibReset());
  assert(calibIsValid() && nvs.travel==1000);
  assert(!calibFinishAtBottom(-2200));
  assert(floorGetFullTravelSteps()==1000);
  nvs.failWrite=false;
  floorInit();
  assert(calibIsValid() && floorGetFullTravelSteps()==1000);
  assert(!calibFinishAtBottom(1200));
  assert(!calibFinishAtBottom(-100));
  assert(calibReset());
  floorInit();
  assert(!calibIsValid() && nvs.travel==0);
  nvs.failOpen=true;
  assert(!calibFinishAtBottom(-1200));
  puts("Calibration storage: PASS");
}
