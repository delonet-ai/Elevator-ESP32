#pragma once
#include <stdint.h>
#include <stdlib.h>
class Stream;
unsigned long millis();
struct TestSerial { template<typename... Args> void printf(const char*, Args...) {} };
extern TestSerial Serial;
