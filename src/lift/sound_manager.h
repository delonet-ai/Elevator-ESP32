#pragma once
#include "sound_engine.h"
struct SoundRequest {
  SoundSettings settings;
  uint32_t revision,receivedAt;
  bool test;
};
struct SoundSnapshot {
  SoundSettings settings;
  uint32_t revision,errors,discarded;
  uint16_t argument;
  uint8_t storage,active,last,queued;
  bool ready;
};
// Default backend is deliberately disconnected; no audio hardware is assumed.
void soundInit(SoundBackend *backend=nullptr);
void soundUpdate();
void soundEmit(SoundCue cue,uint16_t argument=0);
SoundSnapshot soundSnapshot();
// Main-loop only. 2 saved, 3 rejected, 4 NVS failure, 5 test queued, 6 no module.
uint8_t soundApply(const SoundRequest &request,uint32_t now,bool idle);
