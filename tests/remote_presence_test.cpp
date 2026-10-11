#include "../src/common/remote_presence.h"
#include <assert.h>
int main() {
  RemotePresence p;
  p.poll(100);
  assert(!p.received() && !p.online());
  p.observe(0, 0); // A packet at uptime zero is real, not a sentinel.
  assert(p.received() && p.online());
  p.poll(2999); assert(p.online());
  p.poll(3000); assert(!p.online());
  p.observe(3100, 3200); assert(p.online() && p.age() == 100);
  p.poll(6100); assert(!p.online());
  p.observe(6200, 9500); assert(!p.online()); // Queue delay counts as silence.
  p.observe(UINT32_MAX - 99, UINT32_MAX - 49);
  p.poll(50); assert(p.online() && p.age() == 150);
  p.poll(3000); assert(!p.online() && p.age() == 3100);
  p.poll(UINT32_MAX); p.poll(10); assert(p.age() == UINT32_MAX && !p.online());
  p.observe(11, 12); assert(p.online() && p.age() == 1);
}
