#pragma once
#include <stdint.h>

struct MotionSettings {
  uint32_t minimum, maximum, manual, homing, down, acceleration;
};
static constexpr uint32_t MOTION_AUTO_MIN = 50;
static constexpr uint32_t MOTION_AUTO_MAX = 10000;
inline MotionSettings motionDefaults() { return {200, 2000, 400, 400, 1200, 1800}; }
inline bool motionValid(const MotionSettings &s) {
  return s.minimum >= MOTION_AUTO_MIN && s.minimum <= s.maximum && s.maximum <= MOTION_AUTO_MAX &&
    s.manual >= 200 && s.manual <= 2000 && s.homing >= 200 && s.homing <= 2000 &&
    s.down >= 200 && s.down <= 2000 && s.acceleration >= 100 && s.acceleration <= 1800;
}
struct MotionRequest {
  MotionSettings settings;
  uint32_t revision, receivedAt;
};
void motionSettingsInit();
const MotionSettings &motionSettings();
uint32_t motionRevision();
// 0: defaults (no record), 1: loaded/saved, 2: unreadable/invalid record.
uint8_t motionStorageStatus();
// Main control loop only. Returns 2 saved, 3 unavailable/stale, 4 storage error.
uint8_t motionApply(const MotionRequest &request, uint32_t now, bool idle);
