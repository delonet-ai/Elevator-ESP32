#pragma once
#include <stdint.h>

enum class WebAction : uint8_t { Start, Heartbeat, Down, Pause, Save, Reset, Home, Clear };
struct WebCommand {
  WebAction action;
  uint32_t owner, generation, sequence, receivedAt;
};

// Pure policy, shared by firmware and host tests. No timers can resurrect an
// expired lease: expiration must be checked before consuming queued commands.
struct WebControlLease {
  static constexpr uint32_t TIMEOUT_MS = 450;
  static constexpr uint32_t MAX_COMMAND_AGE_MS = 250;
  uint32_t owner = 0;
  uint32_t generation = 1;
  uint32_t sequence = 0;
  uint32_t renewedAt = 0;

  bool active() const { return owner != 0; }
  bool expired(uint32_t now) const { return active() && uint32_t(now-renewedAt) >= TIMEOUT_MS; }
  bool fresh(const WebCommand &cmd, uint32_t now) const {
    return cmd.owner && cmd.sequence && cmd.generation == generation &&
           uint32_t(now-cmd.receivedAt) <= MAX_COMMAND_AGE_MS;
  }
  bool claim(const WebCommand &cmd, uint32_t now) {
    if (active() || !fresh(cmd, now)) return false;
    owner = cmd.owner;
    sequence = cmd.sequence;
    renewedAt = cmd.receivedAt;
    return true;
  }
  bool accept(const WebCommand &cmd, uint32_t now) {
    if (!active() || expired(now) || !fresh(cmd, now) || cmd.owner != owner || cmd.sequence <= sequence) return false;
    sequence = cmd.sequence;
    renewedAt = cmd.receivedAt;
    return true;
  }
  void revoke() {
    owner = sequence = renewedAt = 0;
    if (++generation == 0) ++generation;
  }
};
