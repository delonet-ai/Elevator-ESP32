#include "remote_comm.h"
#include "config.h"
#include "espnow_compat.h"
#include "log.h"

#include <WiFi.h>
#include <esp_wifi.h>

namespace {

const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

uint8_t g_baseMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
volatile bool g_baseKnown = false;

LiftStatus g_status = {};
volatile unsigned long g_lastStatusMs = 0;
volatile bool g_pendingBaseMac = false;
uint8_t g_pendingMac[6] = {0};

uint16_t g_seq = 0;
unsigned long g_lastDiscoverMs = 0;
bool g_linkUp = false;

bool addPeer(const uint8_t *mac) {
  if (esp_now_is_peer_exist(mac)) return true;

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  peer.ifidx   = WIFI_IF_STA;

  esp_err_t err = esp_now_add_peer(&peer);
  if (err != ESP_OK) {
    LOG_E("[COMM] add_peer failed: %d", (int)err);
    return false;
  }
  return true;
}

ESPNOW_SEND_CB(onSent) {
  (void)status;
  // Тихо: печать в колбэке блокирует задачу Wi-Fi.
}

ESPNOW_RECV_CB(onRecv) {
  if (len != (int)sizeof(LiftStatus)) return;
  if (!protoHeaderValid(data)) return;

  memcpy(&g_status, data, sizeof(g_status));
  g_lastStatusMs = millis();

  const uint8_t *src = ESPNOW_RECV_SRC_MAC;
  if (src != nullptr && (!g_baseKnown || memcmp(g_baseMac, src, 6) != 0)) {
    memcpy(g_pendingMac, src, 6);
    g_pendingBaseMac = true;
  }
}

}  // namespace

bool commInit() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  LOG_I("[COMM] Remote MAC %02X:%02X:%02X:%02X:%02X:%02X, channel %u",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ESPNOW_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    LOG_E("[COMM] esp_now_init failed");
    return false;
  }

  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onRecv);

  if (!addPeer(BROADCAST_MAC)) return false;

  LOG_I("[COMM] ESP-NOW ready, searching for base");
  return true;
}

void commUpdate() {
  // Регистрацию peer делаем в основном цикле, а не в колбэке Wi-Fi.
  if (g_pendingBaseMac) {
    g_pendingBaseMac = false;
    if (addPeer(g_pendingMac)) {
      memcpy(g_baseMac, g_pendingMac, 6);
      g_baseKnown = true;
      LOG_I("[COMM] Base found: %02X:%02X:%02X:%02X:%02X:%02X",
            g_baseMac[0], g_baseMac[1], g_baseMac[2],
            g_baseMac[3], g_baseMac[4], g_baseMac[5]);
    }
  }

  unsigned long now = millis();
  bool fresh = (g_lastStatusMs != 0) && (now - g_lastStatusMs < LINK_TIMEOUT_MS);

  if (fresh != g_linkUp) {
    g_linkUp = fresh;
    LOG_I("[COMM] Link %s", g_linkUp ? "UP" : "LOST");
  }

  if (!g_linkUp && (now - g_lastDiscoverMs >= DISCOVER_PERIOD_MS)) {
    g_lastDiscoverMs = now;
    // База молчит: возможно, перезагрузилась и забыла адрес пульта.
    // Широковещательный запрос заставляет её снова нас зарегистрировать.
    g_baseKnown = false;
    memcpy(g_baseMac, BROADCAST_MAC, 6);
    commSend(CMD_DISCOVER, 0);
  }
}

void commSend(uint8_t type, uint8_t arg) {
  RemoteCommand cmd;
  cmd.magic   = PROTO_MAGIC;
  cmd.version = PROTO_VERSION;
  cmd.type    = type;
  cmd.arg     = arg;
  cmd.seq     = ++g_seq;

  const uint8_t *dst = g_baseKnown ? g_baseMac : BROADCAST_MAC;
  esp_now_send(dst, (uint8_t *)&cmd, sizeof(cmd));
}

bool commHasLink() {
  return g_linkUp;
}

const LiftStatus &commStatus() {
  return g_status;
}
