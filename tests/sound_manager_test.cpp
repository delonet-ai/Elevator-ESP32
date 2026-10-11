#include <assert.h>
#include "../src/lift/sound_manager.cpp"
TestNvs nvs;
unsigned long millis(){return 100;}
int main(){
  soundInit();assert(!soundSnapshot().ready&&soundSnapshot().storage==0);
  SoundRequest r={{1,0,0},1,100,false};
  assert(soundApply(r,100,false)==3);assert(soundApply(r,2101,true)==3);
  nvs.failWrite=true;assert(soundApply(r,100,true)==4);
  assert(soundSnapshot().revision==1&&soundSnapshot().settings.volume==60);
  nvs.failWrite=false;assert(soundApply(r,100,true)==2);
  assert(soundSnapshot().revision==2&&soundSnapshot().settings.volume==0);
  assert(soundApply(r,100,true)==3);
  soundInit();assert(soundSnapshot().storage==1&&soundSnapshot().settings.volume==0);
  r.test=true;assert(soundApply(r,100,true)==6);
  soundEmit(SoundCue::Arrival,500);soundUpdate();
  assert(soundSnapshot().argument==500&&soundSnapshot().queued==0);
  nvs.blob[8]^=1;soundInit();assert(soundSnapshot().storage==2&&soundSnapshot().settings.volume==60);
  nvs.failOpen=true;soundInit();assert(soundSnapshot().storage==2);
}
