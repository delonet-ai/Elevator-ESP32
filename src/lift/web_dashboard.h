#pragma once
#include <stdint.h>
#include "web_control_policy.h"
#include "motion_settings.h"
#include "event_journal.h"
#include "sound_manager.h"

// Immutable copies cross from the control loop into the HTTP task. Never
// call the state machine, motor or Preferences from an HTTP callback.
struct WebSnapshot {
  uint8_t state, floor, target, error, speed, channel;
  bool known, top, peer, stationary, running;
  int32_t position, travel;
  uint32_t uptime, freeHeap;
  int32_t rssi;
  char ip[16];
  uint32_t calibOwner, calibGeneration;
  uint8_t calibResult;
  bool calibCanStart, calibCanSave;
  MotionSettings motion;
  uint32_t motionRevision;
  uint8_t motionStorage;
  JournalSnapshot journal;
  bool remoteSeen, remoteOnline;
  uint32_t remoteAgeMs;
  SoundSnapshot sound;
};

bool dashboardInit(const char *password);
void dashboardStart();
void dashboardStop();
void dashboardPublish(const WebSnapshot &snapshot);
bool dashboardTakeNetworkRequest(uint32_t &receivedAt);
void dashboardRejectNetworkRequest();
bool dashboardTakeCommand(WebCommand &command);
bool dashboardTakeStopRequest();

bool dashboardTakeMotionRequest(MotionRequest &request);
void dashboardMotionResult(uint8_t result);

bool dashboardTakeSoundRequest(SoundRequest &request);
void dashboardSoundResult(uint8_t result);
