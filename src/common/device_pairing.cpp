#include "device_pairing.h"
#include <Preferences.h>
#include <esp_wifi.h>
#include <mbedtls/md.h>
namespace {
struct Pair {uint32_t version;uint8_t mac[6];uint8_t key[16];} pair={};
bool valid=false;
int hex(char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
bool decode(const char *s,uint8_t *out,size_t n){for(size_t i=0;i<n;++i){int a=hex(s[2*i]),b=hex(s[2*i+1]);if(a<0||b<0)return false;out[i]=(a<<4)|b;}return true;}
bool sane(const Pair &p){uint8_t bits=0,keyBits=0;for(auto b:p.mac)bits|=b;for(auto b:p.key)keyBits|=b;return p.version==1&&bits&&!(p.mac[0]&1)&&keyBits;}
}
void pairingInit(){Preferences p;valid=false;if(p.begin("lift-pair",true)){valid=p.getBytesLength("pair")==sizeof(pair)&&p.getBytes("pair",&pair,sizeof(pair))==sizeof(pair)&&sane(pair);p.end();}}
bool pairingReady(){return valid;}
const uint8_t *pairingMac(){return pair.mac;}
bool pairingAccept(const uint8_t *mac){return mac&&(!valid||!memcmp(mac,pair.mac,6));}
void pairingConfigurePeer(esp_now_peer_info_t &peer){if(valid&&!memcmp(peer.peer_addr,pair.mac,6)){peer.encrypt=true;memcpy(peer.lmk,pair.key,16);}}
bool pairingStartRadio(){return !valid||esp_now_set_pmk(pair.key)==ESP_OK;}
bool pairingCommand(const char *line){
  if(strncmp(line,"PAIR ",5))return false;
  Pair next={};next.version=1;
  if(strlen(line)!=50||line[17]!=' '||!decode(line+5,next.mac,6)||!decode(line+18,next.key,16)||!sane(next)){Serial.println("PAIR INVALID");return true;}
  Preferences p;bool saved=false;if(p.begin("lift-pair",false)){saved=p.putBytes("pair",&next,sizeof(next))==sizeof(next);p.end();}
  memset(&next,0,sizeof(next));Serial.println(saved?"PAIR SAVED; REBOOT REQUIRED":"PAIR STORAGE ERROR");return true;
}
void deviceHostname(char *out,size_t size,const char *role){uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_STA);snprintf(out,size,"lift-%s-%02x%02x%02x",role,mac[3],mac[4],mac[5]);}
void pairingTag(const void *data,size_t size,uint8_t *tag){uint8_t full[32];mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),pair.key,16,static_cast<const uint8_t*>(data),size,full);memcpy(tag,full,16);memset(full,0,sizeof(full));}
bool pairingVerify(const void *data,size_t size,const uint8_t *tag){uint8_t expected[16];pairingTag(data,size,expected);uint8_t diff=0;for(unsigned i=0;i<16;++i)diff|=expected[i]^tag[i];return valid&&diff==0;}
