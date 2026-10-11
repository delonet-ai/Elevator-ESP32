#pragma once
#include "protocol.h"

// Updated in the control loop only. Saturation prevents very old traffic from
// looking recent again after millis wraps. Receipt time includes queue delay.
class RemotePresence {
 public:
  void observe(uint32_t receivedAt, uint32_t now) {
    seen = true; ageMs = now - receivedAt; updatedAt = now;
  }
  void poll(uint32_t now) {
    if (seen) {
      const uint32_t elapsed = now - updatedAt;
      ageMs = elapsed > UINT32_MAX - ageMs ? UINT32_MAX : ageMs + elapsed;
    }
    updatedAt = now;
  }
  bool received() const { return seen; }
  bool online() const { return seen && ageMs < REMOTE_PRESENCE_TIMEOUT_MS; }
  uint32_t age() const { return ageMs; }
 private:
  bool seen = false;
  uint32_t ageMs = 0, updatedAt = 0;
};
