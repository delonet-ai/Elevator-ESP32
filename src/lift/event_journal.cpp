#include "event_journal.h"
#include "motor_controller.h"
#include <Arduino.h>
namespace { EventJournal journal; }
void journalRecord(EventKind kind, uint32_t value) {
  journal.record(kind, value, millis(), motorGetPosition());
}
JournalSnapshot journalSnapshot() { return journal.snapshot(); }
