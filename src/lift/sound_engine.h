#pragma once
#include <stdint.h>

// Stable semantic IDs. Backends map these to tones/files/voice, not vice versa.
enum class SoundCue : uint8_t {
  None=0, Boot, Departure, Arrival, CalibrationStart, CalibrationSaved,
  HomeReady, Error, Stop, DoorOpening, DoorClosing, DoorBlocked, Emergency, Test
};
struct SoundSettings { uint32_t enabled, routine, volume; };
inline SoundSettings soundDefaults() { return {1,1,60}; }
inline bool soundValid(const SoundSettings &s) { return s.enabled<=1 && s.routine<=1 && s.volume<=100; }
struct SoundItem { SoundCue cue; uint16_t argument; uint32_t createdAt; };
inline uint8_t soundPriority(SoundCue cue) {
  return cue==SoundCue::Emergency?4:
    (cue==SoundCue::Error || cue==SoundCue::Stop || cue==SoundCue::DoorBlocked)?3:1;
}
// Every call must be bounded and nonblocking: no waits for files/UART/BUSY.
// A backend reports busy from successful start until completion, including
// hardware startup latency. It must own only its configured audio pins.
class SoundBackend {
 public:
  virtual ~SoundBackend() = default;
  virtual void poll(uint32_t now) = 0;
  virtual bool ready() const = 0;
  virtual bool busy() const = 0;
  virtual bool start(const SoundItem &item, uint8_t volume) = 0;
  virtual void stop() = 0;
};
class SoundEngine {
 public:
  static constexpr uint8_t CAPACITY=8;
  void configure(const SoundSettings &settings, SoundBackend &backend) {
    config=settings;count=0;
    if (playing) backend.stop();
    playing=false;
  }
  bool enqueue(SoundCue cue, uint16_t argument, uint32_t now) {
    if (cue==SoundCue::None || static_cast<uint8_t>(cue)>static_cast<uint8_t>(SoundCue::Test)) return false;
    lastCue=cue;lastArgument=argument;
    if (!config.enabled || !config.volume || (!config.routine && soundPriority(cue)<3 && cue!=SoundCue::Test)) return false;
    if (playing && current.cue==cue && current.argument==argument) return false;
    for (uint8_t i=0;i<count;++i) if(queue[i].cue==cue && queue[i].argument==argument)return false;
    if (soundPriority(cue)>=3) {
      // Do not announce an old departure/arrival after STOP or a fault.
      for(uint8_t i=0;i<count;) { if(soundPriority(queue[i].cue)<3)erase(i);else ++i; }
    }
    if(count==CAPACITY) {
      uint8_t lowest=0;
      for(uint8_t i=1;i<count;++i)if(soundPriority(queue[i].cue)<soundPriority(queue[lowest].cue))lowest=i;
      if(soundPriority(cue)<=soundPriority(queue[lowest].cue)){++dropped;return false;}
      erase(lowest);++dropped;
    }
    queue[count++]={cue,argument,now};return true;
  }
  void tick(uint32_t now, SoundBackend &backend) {
    backend.poll(now);
    if(!backend.ready()) {
      if(playing){backend.stop();++faults;}
      playing=false;dropped+=count;count=0;return;
    }
    for(uint8_t i=0;i<count;) {
      if(uint32_t(now-queue[i].createdAt)>(soundPriority(queue[i].cue)>=3?5000u:2000u)){erase(i);++dropped;}
      else ++i;
    }
    if(playing && uint32_t(now-startedAt)>=15000){backend.stop();playing=false;count=0;++faults;return;}
    if(playing && !backend.busy())playing=false;
    if(!count)return;
    uint8_t next=0;
    for(uint8_t i=1;i<count;++i)if(soundPriority(queue[i].cue)>soundPriority(queue[next].cue))next=i;
    if(playing) {
      if(soundPriority(queue[next].cue)<=soundPriority(current.cue))return;
      backend.stop();playing=false;
    }
    current=queue[next];erase(next);
    if(backend.start(current,static_cast<uint8_t>(config.volume))){playing=true;startedAt=now;}
    else {++faults;count=0;}
  }
  uint8_t queued() const { return count; }
  SoundCue active() const { return playing?current.cue:SoundCue::None; }
  SoundCue last() const { return lastCue; }
  uint16_t argument() const { return lastArgument; }
  uint32_t errors() const { return faults; }
  uint32_t discarded() const { return dropped; }
 private:
  void erase(uint8_t at){for(uint8_t i=at;i+1<count;++i)queue[i]=queue[i+1];--count;}
  SoundSettings config=soundDefaults();
  SoundItem queue[CAPACITY]={},current={SoundCue::None,0,0};
  uint8_t count=0;
  bool playing=false;
  uint32_t startedAt=0,faults=0,dropped=0;
  SoundCue lastCue=SoundCue::None;
  uint16_t lastArgument=0;
};
