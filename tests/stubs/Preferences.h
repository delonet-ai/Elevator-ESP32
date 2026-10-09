#pragma once
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <string.h>
struct TestNvs {
  bool failOpen = false, failWrite = false;
  uint32_t version = 0;
  int32_t travel = 0;
  std::vector<uint8_t> blob;
};
extern TestNvs nvs;
class Preferences {
public:
  bool begin(const char*, bool) { return !nvs.failOpen; }
  void end() {}
  size_t getBytesLength(const char*) { return nvs.blob.size(); }
  size_t getBytes(const char*, void *out, size_t size) {
    if (size != nvs.blob.size()) return 0;
    memcpy(out, nvs.blob.data(), size); return size;
  }
  size_t putBytes(const char*, const void *data, size_t size) {
    if (nvs.failWrite) return 0;
    const auto *bytes = static_cast<const uint8_t *>(data);
    nvs.blob.assign(bytes, bytes + size); return size;
  }
  uint32_t getUInt(const char*, uint32_t) { return nvs.version; }
  int32_t getLong(const char*, int32_t) { return nvs.travel; }
  size_t putUInt(const char*, uint32_t value) { if(nvs.failWrite)return 0;nvs.version=value;return 4; }
  size_t putLong(const char*, int32_t value) { if(nvs.failWrite)return 0;nvs.travel=value;return 4; }
};
