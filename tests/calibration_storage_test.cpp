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
  assert(calibIsValid() && floorGetFullTravelSteps()==1000);
  assert(!calibFinishAtBottom(-2200));
  assert(floorGetFullTravelSteps()==1000);
  nvs.failWrite=false;
  floorInit();
  assert(calibIsValid() && floorGetFullTravelSteps()==1000);
  assert(!calibFinishAtBottom(1200));
  assert(!calibFinishAtBottom(-100));
  assert(calibReset());
  floorInit();
  assert(!calibIsValid() && floorGetFullTravelSteps()==0);
  // Upgrade v1 without moving its endpoints, then persist a custom middle.
  nvs.blob.clear();nvs.version=1;nvs.travel=1000;floorInit();
  assert(floorGetPositionForFloor(2)==500 && !floorMiddleCustom());
  MiddleRequest request={100, floorRevision(), 370, false};
  assert(floorApplyMiddle(request,100,false,370)==3); // busy/unknown position
  assert(floorApplyMiddle(request,100,true,371)==3); // moved since snapshot
  assert(floorApplyMiddle(request,1101,true,370)==3); // stale
  request.revision++;assert(floorApplyMiddle(request,100,true,370)==3);request.revision--;
  request.position=0;assert(floorApplyMiddle(request,100,true,0)==3);
  request.position=990;assert(floorApplyMiddle(request,100,true,990)==3);
  request.position=370;
  nvs.failWrite=true;assert(floorApplyMiddle(request,100,true,370)==4);
  assert(floorGetPositionForFloor(2)==500);nvs.failWrite=false;
  assert(floorApplyMiddle(request,100,true,370)==2);
  assert(floorMiddleCustom() && floorGetPositionForFloor(2)==370);
  assert(floorApplyMiddle(request,100,true,370)==3); // replay
  floorInit();assert(floorMiddleCustom() && floorGetPositionForFloor(2)==370);
  assert(floorGetPositionForFloor(1)==0 && floorGetPositionForFloor(3)==1000);
  request={100,floorRevision(),0,true};assert(floorApplyMiddle(request,100,true,0)==2);
  floorInit();assert(!floorMiddleCustom() && floorGetPositionForFloor(2)==500);
  request={100,floorRevision(),630,false};assert(floorApplyMiddle(request,100,true,630)==2);
  assert(floorSetFullTravelSteps(1000)); // even same travel invalidates old middle
  floorInit();assert(!floorMiddleCustom() && floorGetPositionForFloor(2)==500);
  assert(!floorSetFullTravelSteps(CALIB_MAX_TRAVEL_STEPS+1));
  assert(floorClearCalibration());floorInit();assert(!calibIsValid()); // no v1 resurrection
  nvs.blob.assign(1,255);floorInit();assert(!calibIsValid()); // corrupt record wins over v1
  nvs.failOpen=true;
  assert(!calibFinishAtBottom(-1200));
  puts("Calibration storage: PASS");
}
