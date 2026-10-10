#include "../src/lift/comm_lift.cpp"
#include <assert.h>
FakeRadio radio;
RadioWiFi WiFi;
TestSerial Serial;
static uint32_t now=100;
static unsigned movements=0,stops=0;
static std::vector<uint32_t> linkEvents;
unsigned long millis() { return now; }
void journalRecord(EventKind kind,uint32_t value) { if(kind==EventKind::RemoteLink)linkEvents.push_back(value); }
LiftState smGetState() { return STATE_IDLE; }
uint8_t smGetCurrentFloor() { return 1; }
uint8_t smGetTargetFloor() { return 0; }
int8_t smGetDirection() { return 0; }
uint8_t smGetError() { return 0; }
uint8_t ioSpeedPercent() { return 50; }
void smCommandMoveToFloor(uint8_t) { ++movements; }
void smCommandCalibDownSave() { ++movements; }
void smCommandManualUpHold() { ++movements; }
void smCommandManualDownHold() { ++movements; }
void smCommandManualStop() { ++stops; }
void smCommandStop() { ++stops; }
void smCommandStartCalib() { ++movements; }
void smCommandCalibDownHold() { ++movements; }
void smCommandClearError() {}
void smCommandStartHoming() { ++movements; }
int main() {
  assert(commInit()); assert(!commRemoteSeen() && !commRemoteOnline());
  const uint8_t a[6]={1,2,3,4,5,6}, b[6]={6,5,4,3,2,1};
  RemoteCommand packet={PROTO_MAGIC,PROTO_VERSION,CMD_DISCOVER,0,1};
  radio.receive(a,reinterpret_cast<uint8_t*>(&packet),sizeof(packet));commPoll();
  assert(commHasPeer() && commRemoteOnline() && movements==0);
  assert(linkEvents.size()==1 && linkEvents[0]==1);
  now=3100;commPoll();assert(!commRemoteOnline() && linkEvents.back()==0);
  radio.receive(a,reinterpret_cast<uint8_t*>(&packet),sizeof(packet));
  now=3400;commPoll();assert(!commRemoteOnline()); // Old discovery cannot revive presence.
  packet.type=CMD_MANUAL_UP;
  radio.receive(a,reinterpret_cast<uint8_t*>(&packet),sizeof(packet));
  now+=300;commPoll();assert(movements==0);
  packet.type=CMD_STOP;
  radio.receive(a,reinterpret_cast<uint8_t*>(&packet),sizeof(packet));
  now+=300;commPoll();assert(stops==1 && !commRemoteOnline());
  packet.type=255;
  radio.receive(b,reinterpret_cast<uint8_t*>(&packet),sizeof(packet));commPoll();assert(!commRemoteOnline());
  packet.type=CMD_DISCOVER;
  radio.receive(a,reinterpret_cast<uint8_t*>(&packet),sizeof(packet));
  radio.receive(b,reinterpret_cast<uint8_t*>(&packet),sizeof(packet));commPoll();
  assert(commRemoteOnline());commSendStatusIfDue();
  assert(!memcmp(radio.sent.back().destination.data(),b,6)); // Each source stays with its packet.
  assert(movements==0);
}
