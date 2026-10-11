#include <assert.h>
#include "../src/common/maintenance_protocol.h"
#include "../src/common/ota_package.h"
int main(){
  uint8_t nonce[16]={1};MaintenanceMessage m;m.type=2;m.ota=1;memcpy(m.nonce,nonce,16);
  strcpy(m.ssid,"Home");strcpy(m.admin,"Poiu0987");
  assert(maintenanceResponseValid(m,nonce,100,101));
  strcpy(m.admin,"short");assert(!maintenanceResponseValid(m,nonce,100,101));strcpy(m.admin,"Poiu0987");
  assert(!maintenanceResponseValid(m,nonce,100,3100));
  assert(!maintenanceResponseValid(m,nonce,0,101)); // consumed challenge / replay
  m.nonce[0]^=1;assert(!maintenanceResponseValid(m,nonce,100,101));m.nonce[0]^=1;
  m.admin[32]='x';assert(!maintenanceResponseValid(m,nonce,100,101));m.admin[32]=0;
  m.wifiPassword[64]='x';assert(!maintenanceResponseValid(m,nonce,100,101));m.wifiPassword[64]=0;
  assert(maintenanceResponseValid(m,nonce,UINT32_MAX-100,100));
  OtaHeader h={};memcpy(h.magic,"LIFTOTA1",8);h.role=1;h.size=1024;
  assert(otaHeaderValid(h,1,0x140000));assert(!otaHeaderValid(h,2,0x140000));
  h.size=0x140001;assert(!otaHeaderValid(h,1,0x140000));
  h.size=0;assert(!otaHeaderValid(h,1,0x140000));
  h.size=1024;h.reserved[0]=1;assert(!otaHeaderValid(h,1,0x140000));
}
