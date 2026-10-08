#pragma once
#include <stdint.h>
#include <stddef.h>
struct TestNvs {
  bool failOpen = false, failWrite = false;
  uint32_t version = 0;
  int32_t travel = 0;
};
extern TestNvs nvs;
class Preferences {
public:
  bool begin(const char*, bool) { return !nvs.failOpen; }
  void end() {}
  uint32_t getUInt(const char*, uint32_t) { return nvs.version; }
  int32_t getLong(const char*, int32_t) { return nvs.travel; }
  size_t putUInt(const char*, uint32_t value) { if(nvs.failWrite)return 0;nvs.version=value;return 4; }
  size_t putLong(const char*, int32_t value) { if(nvs.failWrite)return 0;nvs.travel=value;return 4; }
};
