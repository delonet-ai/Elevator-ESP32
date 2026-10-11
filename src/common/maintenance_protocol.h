#pragma once
#include <stdint.h>
#include <string.h>
struct MaintenanceMessage {
  uint8_t magic=0xD7,version=1,type=0,ota=0;
  uint8_t nonce[16]={};
  char ssid[33]={},wifiPassword[65]={},admin[33]={},ip[16]={};
  uint8_t tag[16]={};
};
static_assert(sizeof(MaintenanceMessage)<=250,"ESP-NOW v1 payload");
inline bool maintenanceResponseValid(const MaintenanceMessage &m,const uint8_t *nonce,uint32_t sentAt,uint32_t now){
  return m.magic==0xD7&&m.version==1&&m.type==2&&m.ota<=1&&sentAt&&uint32_t(now-sentAt)<3000&&
    !memcmp(nonce,m.nonce,16)&&m.ssid[32]==0&&m.wifiPassword[64]==0&&m.admin[32]==0&&m.ssid[0]&&strlen(m.admin)>=8;
}
