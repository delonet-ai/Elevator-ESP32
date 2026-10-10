#include "../src/remote/remote_comm.cpp"
#include <assert.h>
FakeRadio radio;
RadioWiFi WiFi;
TestSerial Serial;
static uint32_t now=100;
unsigned long millis() { return now; }
int main() {
  assert(commInit());
  const uint8_t base[6]={1,2,3,4,5,6};
  LiftStatus packet={};packet.magic=PROTO_MAGIC;packet.version=PROTO_VERSION;
  radio.receive(base,reinterpret_cast<uint8_t*>(&packet),sizeof(packet));commUpdate();
  assert(commHasLink());
  now=1100;commUpdate();
  assert(radio.sent.size()==1 && radio.sent[0].bytes[2]==CMD_DISCOVER);
  assert(!memcmp(radio.sent[0].destination.data(),base,6)); // Idle heartbeat is unicast.
  now=1200;commUpdate();assert(radio.sent.size()==1);
  now=2100;commUpdate();assert(!commHasLink());
  assert(radio.sent.back().bytes[2]==CMD_DISCOVER && radio.sent.back().destination[0]==255);
  const auto count=radio.sent.size();commSend(CMD_MANUAL_UP,0);assert(radio.sent.size()==count);
  now=2200;radio.receive(base,reinterpret_cast<uint8_t*>(&packet),sizeof(packet));commUpdate();
  assert(commHasLink());
  for(const auto &sent : radio.sent) assert(sent.bytes[2]==CMD_DISCOVER);
}
