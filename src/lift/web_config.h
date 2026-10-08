#pragma once

void webInit();
void webUpdate();
// Network provisioning owns the radio and may block briefly. All motion
// entry points must refuse commands while this flag is set.
bool webMotionLocked();
void webPrintNetwork();
