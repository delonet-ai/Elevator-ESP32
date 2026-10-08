#pragma once
#include <stdint.h>

// Immutable copies cross from the control loop into the HTTP task. Never
// call the state machine, motor or Preferences from an HTTP callback.
struct WebSnapshot {
  uint8_t state, floor, target, error, speed, channel;
  bool known, top, peer, stationary, running;
  int32_t position, travel;
  uint32_t uptime, freeHeap;
  int32_t rssi;
  char ip[16];
};

bool dashboardInit(const char *password);
void dashboardStart();
void dashboardStop();
void dashboardPublish(const WebSnapshot &snapshot);
bool dashboardTakeNetworkRequest(uint32_t &receivedAt);
void dashboardRejectNetworkRequest();
