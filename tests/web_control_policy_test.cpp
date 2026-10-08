#include "../src/lift/web_control_policy.h"
#include <assert.h>
#include <stdio.h>

int main() {
  WebControlLease lease;
  WebCommand start = {WebAction::Start, 42, 1, 1, 1000};
  assert(lease.claim(start, 1000));
  assert(!lease.claim(start, 1001)); // duplicated POST cannot restart homing
  WebCommand stranger = {WebAction::Down, 99, 1, 2, 1100};
  assert(!lease.accept(stranger, 1100));
  WebCommand down = {WebAction::Down, 42, 1, 2, 1100};
  assert(lease.accept(down, 1100));
  WebCommand pause = {WebAction::Pause, 42, 1, 3, 1200};
  assert(lease.accept(pause, 1200));
  assert(!lease.accept(down, 1201)); // old DOWN arriving after release
  assert(!lease.expired(1649));
  assert(lease.expired(1650));
  WebCommand late = {WebAction::Heartbeat, 42, 1, 4, 1649};
  assert(!lease.accept(late, 1650)); // fresh packet cannot revive expired lease
  lease.revoke();
  start.receivedAt = 1700;
  assert(!lease.claim(start, 1700)); // invalidated generation
  start.generation = lease.generation;
  assert(!lease.claim(start, 1951)); // stale packet cannot begin motion
  assert(lease.claim(start, 1700));
  lease.revoke();

  // Millis rollover: unsigned subtraction remains a bounded interval.
  start.generation = lease.generation;
  start.receivedAt = UINT32_MAX - 100;
  assert(lease.claim(start, UINT32_MAX - 50));
  assert(!lease.expired(348));
  assert(lease.expired(349));
  lease.generation = UINT32_MAX;
  lease.revoke();
  assert(lease.generation == 1);
  start.owner = 0;
  start.generation = lease.generation;
  assert(!lease.claim(start, 0));
  puts("Web calibration policy: PASS");
}
