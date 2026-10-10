#include "../src/lift/event_journal.h"
#include <assert.h>
int main() {
  EventJournal log;
  assert(log.snapshot().count == 0);
  log.record(EventKind::Boot, 0, UINT32_MAX - 10, -300);
  log.record(EventKind::Error, 7, 5, -301);
  auto saved = log.snapshot();
  assert(saved.count == 2 && saved.overwritten == 0);
  assert(saved.entries[0].id == 1 && saved.entries[1].uptime == 5);
  assert(saved.entries[1].kind == static_cast<uint8_t>(EventKind::Error));
  assert(saved.entries[1].value == 7 && saved.entries[1].position == -301);
  for (int i = 0; i < 100; ++i) log.record(EventKind::Stop, i, i + 10, i);
  const auto full = log.snapshot();
  assert(full.count == 32 && full.overwritten == 70);
  assert(full.entries[0].id == 71 && full.entries[31].id == 102);
  for (unsigned i = 1; i < full.count; ++i) assert(full.entries[i].id == full.entries[i-1].id + 1);
  assert(saved.count == 2 && saved.entries[1].value == 7); // Published copies stay immutable.
  EventJournal rebooted;
  rebooted.record(EventKind::Boot, 0, 0, 0);
  assert(rebooted.snapshot().count == 1 && rebooted.snapshot().entries[0].id == 1);
}
