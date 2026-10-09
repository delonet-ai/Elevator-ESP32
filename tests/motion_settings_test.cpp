#include <assert.h>
#include "../src/lift/motion_settings.cpp"
TestNvs nvs;
int main() {
  motionSettingsInit();
  assert(motionStorageStatus() == 0 && motionSettings().maximum == 2000);
  MotionRequest r = {motionDefaults(), motionRevision(), 100};
  r.settings.maximum = 1500;
  assert(motionApply(r, 100, false) == 3); // Motion/calibration wins over queued edit.
  assert(motionApply(r, 2101, true) == 3);
  r.settings.minimum = 1600;
  assert(motionApply(r, 100, true) == 3);
  r.settings.minimum = 300;
  nvs.failOpen = true;
  assert(motionApply(r, 100, true) == 4);
  nvs.failOpen = false; nvs.failWrite = true;
  assert(motionApply(r, 100, true) == 4);
  assert(motionRevision() == 1 && motionSettings().maximum == 2000);
  nvs.failWrite = false;
  assert(motionApply(r, 100, true) == 2);
  assert(motionRevision() == 2 && motionSettings().maximum == 1500);
  assert(motionApply(r, 101, true) == 3); // Second tab/replay cannot overwrite.
  motionSettingsInit();
  assert(motionSettings().minimum == 300 && motionStorageStatus() == 1);
  r.revision = motionRevision(); r.receivedAt = UINT32_MAX - 99;
  assert(motionApply(r, 100, true) == 2); // millis wrap.
  nvs.blob[8] ^= 1;
  motionSettingsInit();
  assert(motionStorageStatus() == 2 && motionSettings().minimum == 200);
  nvs.blob.resize(1);
  motionSettingsInit();
  assert(motionStorageStatus() == 2);
  auto s = motionDefaults(); s.acceleration = 1801; assert(!motionValid(s));
  s = motionDefaults(); s.homing = 199; assert(!motionValid(s));
  s = motionDefaults(); s.down = UINT32_MAX; assert(!motionValid(s));
}
