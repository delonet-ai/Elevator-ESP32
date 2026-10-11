#pragma once
#include <Arduino.h>
void maintenanceInit(const char *password); // base password or empty on remote
bool maintenanceReceive(const uint8_t *mac,const uint8_t *data,int len);
void maintenancePoll(bool idle);
bool maintenanceActive();
bool maintenanceStart(); // main loop only, caller verifies idle and Wi-Fi
void maintenanceInfo(char *out,size_t size);
