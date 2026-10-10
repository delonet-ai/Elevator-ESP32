#include "web_dashboard.h"
#include "web_page.h"
#include <ESPAsyncWebServer.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace {
AsyncWebServer server(80);
String password, token;
String journalBoot;
portMUX_TYPE snapshotMux = portMUX_INITIALIZER_UNLOCKED;
WebSnapshot latest = {};
bool accepting = false;
bool published = false;
uint8_t networkResult = 0; // 0: idle; 1: queued; 2: rejected by control loop.
QueueHandle_t networkQueue = nullptr;
QueueHandle_t commandQueue = nullptr;
bool stopPending = false;
QueueHandle_t motionQueue = nullptr;
uint8_t motionResult = 0; // 1 queued, 2 saved, 3 rejected, 4 storage failure.

bool csrfValid(AsyncWebServerRequest *request) {
  if (request->hasHeader("X-Lift-Token") && request->getHeader("X-Lift-Token")->value() == token) return true;
  request->send(403, "text/plain", "Invalid token");
  return false;
}

bool readNumber(AsyncWebServerRequest *request, const char *key, uint32_t &value) {
  if (!request->hasParam(key, true)) return false;
  String text = request->getParam(key, true)->value();
  if (text.isEmpty() || text.length() > 10) return false;
  uint64_t parsed = 0;
  for (size_t i = 0; i < text.length(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    parsed = parsed * 10 + text[i] - '0';
  }
  if (!parsed || parsed > UINT32_MAX) return false;
  value = (uint32_t)parsed;
  return true;
}

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
  journalBoot = String(esp_random(), HEX) + String(esp_random(), HEX);
  networkQueue = xQueueCreate(1, sizeof(uint32_t));
  commandQueue = xQueueCreate(4, sizeof(WebCommand));
  motionQueue = xQueueCreate(1, sizeof(MotionRequest));
  if (!networkQueue || !commandQueue || !motionQueue) return false;
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authorize(request)) return;
    auto *response = request->beginResponse(200, "text/html; charset=utf-8", PAGE);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authorize(request)) return;
    WebSnapshot s;
    uint8_t result, settingsResult;
    portENTER_CRITICAL(&snapshotMux);
    s = latest;
    result = networkResult;
    settingsResult = motionResult;
    portEXIT_CRITICAL(&snapshotMux);
    char json[1536];
    snprintf(json, sizeof(json),
      "{\"state\":%u,\"floor\":%u,\"target\":%u,\"position\":%ld,\"known\":%s,"
      "\"error\":%u,\"top\":%s,\"speed\":%u,\"peer\":%s,\"channel\":%u,"
      "\"uptime\":%lu,\"ip\":\"%s\",\"token\":\"%s\",\"stationary\":%s,"
      "\"running\":%s,\"travel\":%ld,\"freeHeap\":%lu,\"rssi\":%ld,"
      "\"age\":%lu,\"networkResult\":%u,\"calibOwner\":%lu,\"calibGeneration\":%lu,"
      "\"calibResult\":%u,\"calibCanStart\":%s,\"calibCanSave\":%s,"
      "\"motionRevision\":%lu,\"motionResult\":%u,\"motionStorage\":%u,"
      "\"motion\":{\"minimum\":%lu,\"maximum\":%lu,\"manual\":%lu,\"homing\":%lu,\"down\":%lu,\"acceleration\":%lu}}",
      s.state,s.floor,s.target,(long)s.position,s.known?"true":"false",s.error,
      s.top?"true":"false",s.speed,s.peer?"true":"false",s.channel,
      (unsigned long)s.uptime,s.ip,token.c_str(),s.stationary?"true":"false",
      s.running?"true":"false",(long)s.travel,(unsigned long)s.freeHeap,(long)s.rssi,
      (unsigned long)(millis()-s.uptime),result,(unsigned long)s.calibOwner,
      (unsigned long)s.calibGeneration,s.calibResult,s.calibCanStart?"true":"false",s.calibCanSave?"true":"false",
      (unsigned long)s.motionRevision, settingsResult, s.motionStorage,
      (unsigned long)s.motion.minimum, (unsigned long)s.motion.maximum, (unsigned long)s.motion.manual,
      (unsigned long)s.motion.homing, (unsigned long)s.motion.down, (unsigned long)s.motion.acceleration);
    auto *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-store");
    request->send(response);
  });
  server.on("/api/events", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!authorize(request)) return;
    JournalSnapshot journal;
    uint32_t capturedAt;
    portENTER_CRITICAL(&snapshotMux);
    journal = latest.journal;
    capturedAt = latest.uptime;
    portEXIT_CRITICAL(&snapshotMux);
    auto *response = request->beginResponseStream("application/json");
    response->addHeader("Cache-Control", "no-store");
    response->printf("{\"boot\":\"%s\",\"age\":%lu,\"overwritten\":%lu,\"events\":[",
      journalBoot.c_str(), (unsigned long)(millis()-capturedAt), (unsigned long)journal.overwritten);
    for (uint8_t i = 0; i < journal.count; ++i) {
      const auto &event = journal.entries[i];
      response->printf("%s{\"id\":%lu,\"uptime\":%lu,\"kind\":%u,\"value\":%lu,\"position\":%ld}",
        i ? "," : "", (unsigned long)event.id, (unsigned long)event.uptime, event.kind,
        (unsigned long)event.value, (long)event.position);
    }
    response->print("]}");
    request->send(response);
  });
  server.on("/api/network", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authorize(request)) return;
    if (!csrfValid(request)) return;
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
  server.on("/api/motion", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authorize(request) || !csrfValid(request)) return;
    MotionRequest cmd = {};
    if (!readNumber(request, "revision", cmd.revision) ||
        !readNumber(request, "minimum", cmd.settings.minimum) ||
        !readNumber(request, "maximum", cmd.settings.maximum) ||
        !readNumber(request, "manual", cmd.settings.manual) ||
        !readNumber(request, "homing", cmd.settings.homing) ||
        !readNumber(request, "down", cmd.settings.down) ||
        !readNumber(request, "acceleration", cmd.settings.acceleration) || !motionValid(cmd.settings)) {
      request->send(400, "text/plain; charset=utf-8", "Недопустимые параметры движения"); return;
    }
    cmd.receivedAt = millis();
    bool allowed;
    portENTER_CRITICAL(&snapshotMux);
    allowed = accepting && !stopPending && latest.stationary && !latest.calibOwner &&
      cmd.receivedAt-latest.uptime < 1000 && networkResult != 1 && motionResult != 1 &&
      cmd.revision == latest.motionRevision;
    if (allowed) motionResult = 1;
    portEXIT_CRITICAL(&snapshotMux);
    if (!allowed) { request->send(409, "text/plain; charset=utf-8", "Лифт занят или настройки устарели. Обновите форму."); return; }
    if (xQueueSend(motionQueue, &cmd, 0) != pdTRUE) {
      dashboardMotionResult(3);
      request->send(503, "text/plain", "Queue busy"); return;
    }
    request->send(202, "text/plain", "Queued");
  });
  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authorize(request) || !csrfValid(request)) return;
    portENTER_CRITICAL(&snapshotMux);
    stopPending = true;
    portEXIT_CRITICAL(&snapshotMux);
    request->send(202, "text/plain; charset=utf-8", "Остановка запрошена");
  });
  server.on("/api/calibration", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!authorize(request) || !csrfValid(request)) return;
    WebCommand cmd = {};
    if (!request->hasParam("action", true) ||
        !readNumber(request, "owner", cmd.owner) ||
        !readNumber(request, "generation", cmd.generation) ||
        !readNumber(request, "sequence", cmd.sequence)) {
      request->send(400, "text/plain", "Invalid command"); return;
    }
    String action = request->getParam("action", true)->value();
    if      (action == "start") cmd.action = WebAction::Start;
    else if (action == "heartbeat") cmd.action = WebAction::Heartbeat;
    else if (action == "down") cmd.action = WebAction::Down;
    else if (action == "pause") cmd.action = WebAction::Pause;
    else if (action == "save") cmd.action = WebAction::Save;
    else if (action == "reset") cmd.action = WebAction::Reset;
    else { request->send(400, "text/plain", "Unknown action"); return; }
    cmd.receivedAt = millis();
    bool allowed;
    portENTER_CRITICAL(&snapshotMux);
    allowed = accepting && !stopPending && networkResult != 1 &&
      cmd.receivedAt-latest.uptime < 1000 && cmd.generation == latest.calibGeneration;
    portEXIT_CRITICAL(&snapshotMux);
    if (!allowed) { request->send(409, "text/plain", "Stale or unavailable session"); return; }
    if (xQueueSend(commandQueue, &cmd, 0) != pdTRUE) {
      request->send(503, "text/plain", "Command queue busy"); return;
    }
    request->send(202, "text/plain", "Queued");
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

bool dashboardTakeCommand(WebCommand &command) {
  return commandQueue && xQueueReceive(commandQueue, &command, 0) == pdTRUE;
}

bool dashboardTakeStopRequest() {
  portENTER_CRITICAL(&snapshotMux);
  bool requested = stopPending;
  stopPending = false;
  portEXIT_CRITICAL(&snapshotMux);
  return requested;
}

bool dashboardTakeMotionRequest(MotionRequest &request) {
  return motionQueue && xQueueReceive(motionQueue, &request, 0) == pdTRUE;
}
void dashboardMotionResult(uint8_t result) {
  portENTER_CRITICAL(&snapshotMux);
  motionResult = result;
  portEXIT_CRITICAL(&snapshotMux);
}
