#include "web_dashboard.h"
#include "web_page.h"
#include <ESPAsyncWebServer.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace {
AsyncWebServer server(80);
String password, token;
portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;
WebSnapshot latest = {};
bool accepting = false;
bool published = false;
uint8_t networkResult = 0; // 0: idle; 1: queued; 2: rejected by control loop.
QueueHandle_t networkQueue = nullptr;

bool authorize(AsyncWebServerRequest *request) {
  if (!request->authenticate("admin", password.c_str())) {
    request->requestAuthentication("Lift", false);
    return false;
  }
  bool active;
  portENTER_CRITICAL(&snapshotMux);
  active = accepting && published;
  portEXIT_CRITICAL(&snapshotMux);
  if (!active) request->send(503, "text/plain", "Dashboard not ready");
  return active;
}
}

bool dashboardInit(const char *accessPassword) {
  password = accessPassword;
  token = String(esp_random(), HEX) + String(esp_random(), HEX);
  networkQueue = xQueueCreate(1, sizeof(uint32_t));
  if (!networkQueue) return false;
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authorize(request)) return;
    auto *response = request->beginResponse(200, "text/html; charset=utf-8", PAGE);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authorize(request)) return;
    WebSnapshot s;
    uint8_t result;
    portENTER_CRITICAL(&snapshotMux);
    s = latest;
    result = networkResult;
    portEXIT_CRITICAL(&snapshotMux);
    char json[768];
    snprintf(json, sizeof(json),
      "{\"state\":%u,\"floor\":%u,\"target\":%u,\"position\":%ld,\"known\":%s,"
      "\"error\":%u,\"top\":%s,\"speed\":%u,\"peer\":%s,\"channel\":%u,"
      "\"uptime\":%lu,\"ip\":\"%s\",\"token\":\"%s\",\"stationary\":%s,"
      "\"running\":%s,\"travel\":%ld,\"freeHeap\":%lu,\"rssi\":%ld,"
      "\"age\":%lu,\"networkResult\":%u}",
      s.state,s.floor,s.target,(long)s.position,s.known?"true":"false",s.error,
      s.top?"true":"false",s.speed,s.peer?"true":"false",s.channel,
      (unsigned long)s.uptime,s.ip,token.c_str(),s.stationary?"true":"false",
      s.running?"true":"false",(long)s.travel,(unsigned long)s.freeHeap,(long)s.rssi,
      (unsigned long)(millis()-s.uptime),result);
    auto *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });
  server.on("/api/network", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authorize(request)) return;
    if (!request->hasHeader("X-Lift-Token") || request->getHeader("X-Lift-Token")->value() != token) {
      request->send(403, "text/plain", "Invalid token");
      return;
    }
    const uint32_t now = millis();
    bool allowed;
    portENTER_CRITICAL(&snapshotMux);
    allowed = accepting && latest.stationary && now-latest.uptime < 1000 && networkResult != 1;
    if (allowed) networkResult = 1;
    portEXIT_CRITICAL(&snapshotMux);
    if (!allowed) { request->send(409, "text/plain; charset=utf-8", "Настройка сети сейчас недоступна"); return; }
    if (xQueueSend(networkQueue, &now, 0) != pdTRUE) {
      dashboardRejectNetworkRequest();
      request->send(503, "text/plain", "Request queue busy");
      return;
    }
    request->send(202, "text/plain; charset=utf-8",
      "Запрос принят. После проверки остановки появится точка Lift-Setup; подключитесь к ней и откройте http://192.168.4.1");
  });
  return true;
}

void dashboardPublish(const WebSnapshot &snapshot) {
  portENTER_CRITICAL(&snapshotMux);
  latest = snapshot;
  published = true;
  portEXIT_CRITICAL(&snapshotMux);
}

void dashboardStart() {
  portENTER_CRITICAL(&snapshotMux);
  accepting = true;
  networkResult = 0;
  portEXIT_CRITICAL(&snapshotMux);
  server.begin();
}

void dashboardStop() {
  portENTER_CRITICAL(&snapshotMux);
  accepting = false;
  portEXIT_CRITICAL(&snapshotMux);
  server.end();
}

bool dashboardTakeNetworkRequest(uint32_t &receivedAt) {
  return networkQueue && xQueueReceive(networkQueue, &receivedAt, 0) == pdTRUE;
}

void dashboardRejectNetworkRequest() {
  portENTER_CRITICAL(&snapshotMux);
  networkResult = 2;
  portEXIT_CRITICAL(&snapshotMux);
}
