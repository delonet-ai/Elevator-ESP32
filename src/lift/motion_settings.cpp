#include "motion_settings.h"
#include <Preferences.h>

namespace {
MotionSettings current = motionDefaults();
uint32_t revision = 1;
uint8_t storageStatus = 0;
struct Record { uint32_t version; MotionSettings settings; uint32_t checksum; };
static_assert(sizeof(Record) == 32, "Stable NVS record layout required");
uint32_t checksum(const Record &r) {
  const auto *bytes = reinterpret_cast<const uint8_t *>(&r);
  uint32_t hash = 2166136261u;
  for (unsigned i = 0; i < sizeof(Record) - sizeof(uint32_t); ++i) hash = (hash ^ bytes[i]) * 16777619u;
  return hash;
}
}
void motionSettingsInit() {
  current = motionDefaults(); revision = 1; storageStatus = 0;
  Preferences prefs;
  if (!prefs.begin("lift-motion", false)) { storageStatus = 2; return; }
  const size_t length = prefs.getBytesLength("settings");
  if (length) {
    Record r = {};
    if (length == sizeof(r) && prefs.getBytes("settings", &r, sizeof(r)) == sizeof(r) &&
        r.version == 1 && r.checksum == checksum(r) && motionValid(r.settings)) {
      current = r.settings; storageStatus = 1;
    } else storageStatus = 2;
  }
  prefs.end();
}
const MotionSettings &motionSettings() { return current; }
uint32_t motionRevision() { return revision; }
uint8_t motionStorageStatus() { return storageStatus; }
uint8_t motionApply(const MotionRequest &request, uint32_t now, bool idle) {
  if (!idle || now - request.receivedAt > 2000 || request.revision != revision || !motionValid(request.settings)) return 3;
  Record record = {1, request.settings, 0};
  record.checksum = checksum(record);
  Preferences prefs;
  if (!prefs.begin("lift-motion", false)) return 4;
  const bool saved = prefs.putBytes("settings", &record, sizeof(record)) == sizeof(record);
  prefs.end();
  if (!saved) return 4;
  current = request.settings; storageStatus = 1;
  if (++revision == 0) revision = 1;
  return 2;
}
