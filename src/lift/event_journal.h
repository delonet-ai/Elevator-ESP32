#pragma once
#include <stdint.h>

enum class EventKind : uint8_t {
  Boot = 0, State, Error, Stop, Motor, Top, Calibration, MotionSettings
};
struct JournalEvent {
  uint32_t id, uptime;
  int32_t position;
  uint8_t kind;
  uint32_t value;
};
struct JournalSnapshot {
  static constexpr uint8_t CAPACITY = 32;
  JournalEvent entries[CAPACITY] = {};
  uint32_t overwritten = 0;
  uint8_t count = 0;
};
// Single writer: the control loop. HTTP receives a copy via WebSnapshot.
class EventJournal {
 public:
  void record(EventKind kind, uint32_t value, uint32_t now, int32_t position) {
    data.entries[next] = {sequence++, now, position, static_cast<uint8_t>(kind), value};
    if (!sequence) sequence = 1;
    next = (next + 1) % JournalSnapshot::CAPACITY;
    if (data.count < JournalSnapshot::CAPACITY) ++data.count;
    else if (data.overwritten != UINT32_MAX) ++data.overwritten;
  }
  JournalSnapshot snapshot() const {
    JournalSnapshot result;
    result.count = data.count; result.overwritten = data.overwritten;
    const uint8_t first = data.count == JournalSnapshot::CAPACITY ? next : 0;
    for (uint8_t i = 0; i < data.count; ++i)
      result.entries[i] = data.entries[(first + i) % JournalSnapshot::CAPACITY];
    return result;
  }
 private:
  JournalSnapshot data;
  uint8_t next = 0;
  uint32_t sequence = 1;
};

void journalRecord(EventKind kind, uint32_t value);
JournalSnapshot journalSnapshot();
