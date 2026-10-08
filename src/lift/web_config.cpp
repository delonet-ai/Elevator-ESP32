#include "web_config.h"
#include "state_machine.h"
#include "motor_controller.h"
#include "io_manager.h"
#include "comm_lift.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_system.h>

namespace {
WiFiManager manager;
WebServer dashboard(80);
String apName, password, token;
bool locked = true;
bool serving = false;
bool requestPortal = false;
bool initialized = false;
unsigned long disconnectedAt = 0;

bool stationary() {
  const LiftState state = smGetState();
  return !motorIsRunning() && state != STATE_MOVING &&
         state != STATE_MANUAL_MOVE && state != STATE_HOMING &&
         state != STATE_CALIB_HOMING_UP && state != STATE_CALIB_MOVING_DOWN;
}

bool authorize() {
  if (dashboard.authenticate("admin", password.c_str())) return true;
  dashboard.requestAuthentication();
  return false;
}

const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="ru"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Лифт · Настройки</title>
<style>body{font:16px system-ui;background:#101826;color:#edf3ff;margin:0;padding:24px}main{max-width:760px;margin:auto}h1{margin-bottom:8px}.muted{color:#aabbd4}section{background:#1c293c;border-radius:16px;padding:24px;margin:20px 0}dl{display:grid;grid-template-columns:1fr 1fr;gap:14px}dd{margin:0;text-align:right}button{background:#73d5b6;color:#10251f;border:0;border-radius:8px;padding:14px;font:inherit;cursor:pointer}a{color:#73d5b6}</style>
<main><p class="muted">ELEVATOR ESP32</p><h1>Состояние лифта</h1><p id="connection">Подключение…</p>
<section><dl id="values"></dl></section><section><h2>Подключение к Wi-Fi</h2>
<p>Для смены сети откроется точка Lift-Setup. Пока открыт портал настройки, движение заблокировано.</p>
<button id="network">Настроить другую сеть</button><p id="message"></p></section>
<p class="muted">Во время движения обновление страницы приостановлено. Калибровка и управление мотором в этой версии остаются на пульте и USB.</p></main>
<script>const states=['Запуск','Нужна калибровка','Калибровка вверх','Калибровка вниз','Ожидание','Поездка','Ручное движение','Ошибка','Нужно найти верх','Поиск верха'];let token='';
async function update(){try{const r=await fetch('/api/status',{cache:'no-store',signal:AbortSignal.timeout(2500)});if(!r.ok)throw Error();const s=await r.json();token=s.token;document.getElementById('connection').textContent='База доступна · '+s.ip;
const rows=[['Состояние',states[s.state]||s.state],['Этаж / цель',s.floor+' / '+s.target],['Позиция, шагов',s.position],['Позиция известна',s.known?'Да':'Нет'],['Ошибка',s.error],['Верхний концевик',s.top?'Нажат':'Свободен'],['Регулятор скорости',s.speed+'%'],['Пульт зарегистрирован',s.peer?'Да':'Нет'],['Радиоканал',s.channel],['Время работы, с',Math.floor(s.uptime/1000)]];const box=document.getElementById('values');box.replaceChildren();for(const [k,v]of rows){const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=k;dd.textContent=v;box.append(dt,dd)}}catch(e){document.getElementById('connection').textContent='Нет обновления: движение или сеть недоступна'}setTimeout(update,2000)}update();
document.getElementById('network').onclick=async()=>{if(!token||!confirm('Перейти в режим настройки сети?'))return;try{const r=await fetch('/api/network',{method:'POST',headers:{'X-Lift-Token':token}});document.getElementById('message').textContent=await r.text()}catch(e){document.getElementById('message').textContent='Проверьте наличие точки Lift-Setup'}};</script></html>)HTML";

void startPortal() {
  locked = true;
  dashboard.stop();
  serving = false;
  // Configuration portal must not expose unauthenticated WiFiManager routes
  // to the household LAN. It is reachable only through the protected AP.
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  manager.startConfigPortal(apName.c_str(), password.c_str());
  webPrintNetwork();
}
}

bool webMotionLocked() { return locked; }

void webPrintNetwork() {
  Serial.printf("[WEB] Setup AP: %s password: %s\n", apName.c_str(), password.c_str());
  Serial.printf("[WEB] Dashboard login: admin / same setup password; IP: %s channel: %d\n",
                WiFi.localIP().toString().c_str(), WiFi.channel());
  if (manager.getConfigPortalActive()) Serial.println("[WEB] Setup: http://192.168.4.1 (motion locked)");
}

void webInit() {
  Preferences prefs;
  if (!prefs.begin("lift-web", false)) {
    Serial.println("[WEB] Cannot open credentials storage; motion remains locked");
    return;
  }
  password = prefs.getString("password", "");
  if (password.length() < 12) {
    char generated[17];
    snprintf(generated, sizeof(generated), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
    password = generated;
    if (!prefs.putString("password", password)) {
      prefs.end();
      Serial.println("[WEB] Cannot save setup password; motion remains locked");
      return;
    }
  }
  prefs.end();
  char suffix[7];
  snprintf(suffix, sizeof(suffix), "%06lx", (unsigned long)(ESP.getEfuseMac() & 0xffffff));
  apName = String("Lift-Setup-") + suffix;
  token = String(esp_random(), HEX) + String(esp_random(), HEX);
  manager.setDebugOutput(false);
  manager.setConfigPortalBlocking(false);
  manager.setConnectTimeout(8);
  manager.setSaveConnectTimeout(8);
  manager.setWiFiAutoReconnect(false);
  manager.setConfigPortalTimeout(0);
  std::vector<const char*> menu = {"wifi", "info"};
  manager.setMenu(menu);
  WiFi.setAutoReconnect(false);
  manager.setAPCallback([](WiFiManager*) { locked = true; });
  const char *headers[] = {"X-Lift-Token"};
  dashboard.collectHeaders(headers, 1);
  dashboard.on("/", HTTP_GET, [] {
    if (!authorize()) return;
    dashboard.sendHeader("Cache-Control", "no-store");
    dashboard.send_P(200, "text/html; charset=utf-8", PAGE);
  });
  dashboard.on("/api/status", HTTP_GET, [] {
    if (!authorize()) return;
    char json[640];
    snprintf(json, sizeof(json),
      "{\"state\":%u,\"floor\":%u,\"target\":%u,\"position\":%ld,\"known\":%s,\"error\":%u,\"top\":%s,\"speed\":%u,\"peer\":%s,\"channel\":%d,\"uptime\":%lu,\"ip\":\"%s\",\"token\":\"%s\"}",
      (unsigned)smGetState(), smGetCurrentFloor(), smGetTargetFloor(), smGetPosition(),
      smPositionKnown()?"true":"false", smGetError(), ioTopSwitchActive()?"true":"false",
      ioSpeedPercent(), commHasPeer()?"true":"false", WiFi.channel(), millis(),
      WiFi.localIP().toString().c_str(), token.c_str());
    dashboard.sendHeader("Cache-Control", "no-store");
    dashboard.send(200, "application/json", json);
  });
  dashboard.on("/api/network", HTTP_POST, [] {
    if (!authorize()) return;
    if (dashboard.header("X-Lift-Token") != token) { dashboard.send(403, "text/plain", "Invalid token"); return; }
    if (!stationary()) { dashboard.send(409, "text/plain", "Lift is moving"); return; }
    locked = true;
    requestPortal = true;
    dashboard.send(200, "text/plain; charset=utf-8", "Подключитесь к " + apName + ". Адрес: http://192.168.4.1");
  });
  // Initial connection may wait up to eight seconds, before any commands
  // are serviced and before ESP-NOW is initialized.
  manager.autoConnect(apName.c_str(), password.c_str());
  WiFi.setAutoReconnect(false);
  initialized = true;
  webPrintNetwork();
}

void webUpdate() {
  if (!initialized) return;
  // Never enter synchronous HTTP handlers or WiFiManager scans during motion.
  if (!stationary()) return;
  if (requestPortal) { requestPortal = false; startPortal(); }
  if (manager.getConfigPortalActive()) {
    locked = true;
    manager.process();
    return;
  }
  locked = false;
  if (WiFi.status() == WL_CONNECTED) {
    disconnectedAt = 0;
    if (!serving) {
      dashboard.begin();
      serving = true;
      webPrintNetwork();
    }
    dashboard.handleClient();
  } else {
    if (disconnectedAt == 0) disconnectedAt = millis();
    if (millis() - disconnectedAt > 15000) {
      disconnectedAt = 0;
      startPortal();
    }
  }
}
