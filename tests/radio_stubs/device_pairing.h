#pragma once
#include "esp_now.h"
inline bool pairingReady(){return false;}
inline const uint8_t *pairingMac(){static uint8_t mac[6]={};return mac;}
inline bool pairingAccept(const uint8_t *mac){return mac!=nullptr;}
inline void pairingConfigurePeer(esp_now_peer_info_t&){}
inline bool pairingStartRadio(){return true;}
inline void deviceHostname(char *out,size_t n,const char*){strncpy(out,"lift-remote-010101",n);}
