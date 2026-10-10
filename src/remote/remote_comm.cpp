#include "remote_comm.h"
#include "config.h"
#include "espnow_compat.h"
#include "log.h"
#include <WiFi.h>
#include <esp_wifi.h>

namespace {
const uint8_t BROADCAST_MAC[6] = {255,255,255,255,255,255};
uint8_t baseMac[6] = {};
bool baseKnown = false;
bool linkUp = false;
LiftStatus status = {};
unsigned long lastStatus = 0;
unsigned long lastSearch = 0;
unsigned long lastHeartbeat = 0;
uint16_t sequence = 0;
uint8_t channel = ESPNOW_CHANNEL;
// Callback data is published atomically; all peer and UI work stays in loop().
portMUX_TYPE receiveMux = portMUX_INITIALIZER_UNLOCKED;
LiftStatus pendingStatus = {};
uint8_t pendingMac[6] = {};
unsigned long pendingTime = 0;
bool pending = false;
bool ready = false;

bool addPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0; // Current radio channel, including while searching.
  peer.ifidx = WIFI_IF_STA;
  return esp_now_add_peer(&peer) == ESP_OK;
}

ESPNOW_SEND_CB(onSent) { (void)status; }

ESPNOW_RECV_CB(onRecv) {
  if (len != sizeof(LiftStatus) || !protoHeaderValid(data)) return;
  const uint8_t *src = ESPNOW_RECV_SRC_MAC;
  if (!src) return;
  portENTER_CRITICAL(&receiveMux);
  memcpy(&pendingStatus, data, sizeof(pendingStatus));
  memcpy(pendingMac, src, 6);
  pendingTime = millis();
  pending = true;
  portEXIT_CRITICAL(&receiveMux);
}
}

bool commInit() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  WiFi.disconnect();
  // Search channels allowed in the installation region (1..13). No WiFi scan:
  // each dwell sends DISCOVER, which never requests motor movement.
  wifi_country_t country = {"RU", 1, 13, 20, WIFI_COUNTRY_POLICY_MANUAL};
  esp_wifi_set_country(&country);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() != ESP_OK) return false;
  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onRecv);
  ready = addPeer(BROADCAST_MAC);
  LOG_I("[COMM] Remote MAC %s, channel search 1..13", WiFi.macAddress().c_str());
  return ready;
}

void commUpdate() {
  if (!ready) return;
  LiftStatus received = {};
  uint8_t mac[6];
  unsigned long receivedAt = 0;
  bool available;
  portENTER_CRITICAL(&receiveMux);
  available = pending;
  if (available) {
    received = pendingStatus;
    memcpy(mac, pendingMac, 6);
    receivedAt = pendingTime;
    pending = false;
  }
  portEXIT_CRITICAL(&receiveMux);
  if (available && (!baseKnown || memcmp(baseMac, mac, 6) == 0)) {
    if (!baseKnown && addPeer(mac)) {
      memcpy(baseMac, mac, 6);
      baseKnown = true;
      LOG_I("[COMM] Base found: %02X:%02X:%02X:%02X:%02X:%02X channel %u",
        mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],channel);
    }
    if (baseKnown) { status = received; lastStatus = receivedAt; }
  }
  unsigned long now = millis();
  bool fresh = baseKnown && lastStatus != 0 && now - lastStatus < LINK_TIMEOUT_MS;
  if (fresh != linkUp) {
    linkUp = fresh;
    LOG_I("[COMM] Link %s", linkUp ? "UP" : "LOST");
  }
  if (linkUp && now - lastHeartbeat >= REMOTE_HEARTBEAT_MS) {
    lastHeartbeat = now;
    commSend(CMD_DISCOVER, 0); // Presence only; the base never moves for DISCOVER.
  }
  if (!linkUp && now - lastSearch >= 400) {
    lastSearch = now;
    channel = channel == 13 ? 1 : channel + 1;
    if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) return;
    commSend(CMD_DISCOVER, 0);
  }
}

void commSend(uint8_t type, uint8_t arg) {
  if (!ready) return;
  // No broadcast movement commands while searching or after a stale status.
  if (type != CMD_DISCOVER && (!linkUp || millis() - lastStatus >= LINK_TIMEOUT_MS)) return;
  RemoteCommand cmd = {PROTO_MAGIC, PROTO_VERSION, type, arg, ++sequence};
  const uint8_t *dst = type == CMD_DISCOVER && !linkUp ? BROADCAST_MAC : baseMac;
  esp_now_send(dst, reinterpret_cast<uint8_t*>(&cmd), sizeof(cmd));
}

bool commHasLink() { return linkUp; }
const LiftStatus &commStatus() { return status; }
