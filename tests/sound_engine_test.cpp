#include <assert.h>
#include "../src/lift/sound_engine.h"
struct Output : SoundBackend {
  bool connected=true, running=false, fail=false;
  unsigned starts=0,stops=0; uint8_t volume=0; SoundItem item={};
  void poll(uint32_t) override {}
  bool ready() const override {return connected;}
  bool busy() const override {return running;}
  bool start(const SoundItem &i,uint8_t v) override {item=i;volume=v;++starts;running=!fail;return !fail;}
  void stop() override {++stops;running=false;}
};
int main(){
  Output out;SoundEngine e;
  assert(e.enqueue(SoundCue::Departure,300,0));e.tick(0,out);
  assert(out.item.argument==300&&out.volume==60&&out.starts==1);
  assert(!e.enqueue(SoundCue::Departure,300,1));
  e.enqueue(SoundCue::Arrival,300,1);e.enqueue(SoundCue::Stop,0,2);e.tick(2,out);
  assert(out.item.cue==SoundCue::Stop&&out.stops==1&&e.queued()==0);
  e.enqueue(SoundCue::Emergency,0,3);e.tick(3,out);
  assert(out.item.cue==SoundCue::Emergency&&out.stops==2);
  e.tick(15003,out);assert(e.errors()==1&&e.active()==SoundCue::None);
  e.configure({1,0,50},out);
  assert(!e.enqueue(SoundCue::Arrival,1,0));assert(e.enqueue(SoundCue::Error,1,0));
  e.tick(0,out);assert(out.volume==50);
  e.configure({1,1,0},out);assert(!e.enqueue(SoundCue::Emergency,0,0));
  assert(e.last()==SoundCue::Emergency&&e.active()==SoundCue::None);
  e.configure(soundDefaults(),out);
  for(unsigned i=0;i<8;++i)assert(e.enqueue(SoundCue::Arrival,i,0));
  assert(!e.enqueue(SoundCue::Arrival,8,0));e.tick(2001,out);assert(e.queued()==0);
  assert(e.discarded()==9);
  e.enqueue(SoundCue::Error,1,UINT32_MAX-100);e.tick(100,out);
  assert(e.active()==SoundCue::Error);
  out.connected=false;e.tick(101,out);assert(e.active()==SoundCue::None&&e.errors()==2);
  e.enqueue(SoundCue::Test,0,102);e.tick(102,out);assert(e.queued()==0);
  out.connected=true;out.fail=true;e.enqueue(SoundCue::Test,0,103);e.tick(103,out);
  assert(e.errors()==3&&e.active()==SoundCue::None);
  assert(!soundValid({1,1,101})&&!soundValid({2,1,50}));
}
