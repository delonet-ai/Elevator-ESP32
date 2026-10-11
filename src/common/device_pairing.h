#pragma once
#include <Arduino.h>
#include <esp_now.h>
void pairingInit();
bool pairingReady();
const uint8_t *pairingMac();
bool pairingAccept(const uint8_t *mac);
void pairingConfigurePeer(esp_now_peer_info_t &peer);
bool pairingStartRadio();
bool pairingCommand(const char *line); // USB only, no secret is echoed.
void deviceHostname(char *out, size_t size, const char *role);
void pairingTag(const void *data,size_t size,uint8_t *tag);
bool pairingVerify(const void *data,size_t size,const uint8_t *tag);

bool pairingStore(const char *macHex,const char *keyHex);
