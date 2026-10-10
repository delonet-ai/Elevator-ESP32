#include "sound_manager.h"
#include <Arduino.h>
#include <Preferences.h>
namespace {
class DisconnectedSound : public SoundBackend {
 public:
  void poll(uint32_t) override {}
  bool ready() const override {return false;}
  bool busy() const override {return false;}
  bool start(const SoundItem&,uint8_t) override {return false;}
  void stop() override {}
} disconnected;
SoundBackend *output=&disconnected;
SoundEngine engine;
SoundSettings settings=soundDefaults();
uint32_t revision=1;
uint8_t storage=0;
struct Record {uint32_t version;SoundSettings settings;uint32_t checksum;};
static_assert(sizeof(Record)==20,"Stable audio settings layout");
uint32_t checksum(const Record &record) {
  auto bytes=reinterpret_cast<const uint8_t*>(&record);uint32_t value=2166136261u;
  for(unsigned i=0;i<sizeof(Record)-4;++i)value=(value^bytes[i])*16777619u;
  return value;
}
}
void soundInit(SoundBackend *backend) {
  output=backend?backend:&disconnected;engine=SoundEngine();settings=soundDefaults();revision=1;storage=0;
  Preferences prefs;
  if(!prefs.begin("lift-sound",false))storage=2;
  else {
    const auto size=prefs.getBytesLength("settings");Record record={};
    if(size){
      if(size==sizeof(record)&&prefs.getBytes("settings",&record,sizeof(record))==sizeof(record)&&
        record.version==1&&record.checksum==checksum(record)&&soundValid(record.settings)) {
        settings=record.settings;storage=1;
      }else storage=2;
    }
    prefs.end();
  }
  engine.configure(settings,*output);
}
void soundUpdate(){engine.tick(millis(),*output);}
void soundEmit(SoundCue cue,uint16_t argument){engine.enqueue(cue,argument,millis());}
SoundSnapshot soundSnapshot(){return {settings,revision,engine.errors(),engine.discarded(),engine.argument(),
  storage,static_cast<uint8_t>(engine.active()),static_cast<uint8_t>(engine.last()),engine.queued(),output->ready()};}
uint8_t soundApply(const SoundRequest &request,uint32_t now,bool idle){
  if(!idle||uint32_t(now-request.receivedAt)>2000||request.revision!=revision)return 3;
  if(request.test){
    if(!output->ready())return 6;
    return engine.enqueue(SoundCue::Test,0,now)?5:3;
  }
  if(!soundValid(request.settings))return 3;
  Record record={1,request.settings,0};record.checksum=checksum(record);
  Preferences prefs;if(!prefs.begin("lift-sound",false))return 4;
  const bool saved=prefs.putBytes("settings",&record,sizeof(record))==sizeof(record);prefs.end();
  if(!saved)return 4;
  settings=request.settings;storage=1;if(++revision==0)revision=1;
  engine.configure(settings,*output);return 2;
}
