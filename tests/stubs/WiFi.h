#pragma once
constexpr int WL_CONNECTED = 3;
struct FakeWiFi { int value = WL_CONNECTED; int status() const { return value; } };
extern FakeWiFi WiFi;
