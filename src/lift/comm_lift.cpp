#include "comm_lift.h"
#include "state_machine.h"
#include "io_manager.h"
#include "config.h"
#include "protocol.h"
#include "espnow_compat.h"
#include "log.h"

#include <WiFi.h>
#include <esp_wifi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace {

const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Очередь глубиной 8: команд приходит немного, но при заторе в loop()
// лучше потерять самые старые, чем блокировать задачу Wi-Fi.
QueueHandle_t g_cmdQueue = nullptr;

uint8_t g_remoteMac[6]   = {0};
volatile bool g_haveRemoteMac = false;
bool    g_peerAdded      = false;

unsigned long g_lastStatusMs = 0;

// MAC отправителя последнего пакета: из колбэка в loop() передаём
// через буфер, а не добавляем peer прямо в задаче Wi-Fi.
volatile bool g_pendingPeer = false;
uint8_t g_pendingMac[6] = {0};

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
  // Печать в колбэке блокирует задачу Wi-Fi, поэтому здесь тихо.
}

ESPNOW_RECV_CB(onRecv) {
  if (len != (int)sizeof(RemoteCommand)) return;
  if (!protoHeaderValid(data)) return;

  RemoteCommand cmd;
  memcpy(&cmd, data, sizeof(cmd));

  const uint8_t *src = ESPNOW_RECV_SRC_MAC;
  if (src != nullptr && memcmp(src, BROADCAST_MAC, 6) != 0) {
    if (!g_haveRemoteMac || memcmp(g_remoteMac, src, 6) != 0) {
      memcpy(g_pendingMac, src, 6);
      g_pendingPeer = true;
    }
  }

  if (g_cmdQueue != nullptr) {
    // Не ждём места в очереди: задача Wi-Fi не должна блокироваться.
    xQueueSend(g_cmdQueue, &cmd, 0);
  }
}

void handleCommand(const RemoteCommand &cmd) {
  switch (cmd.type) {
    case CMD_CALL_FLOOR:
      // В калибровке кнопка 1-го этажа фиксирует нижнюю точку.
      if (smGetState() == STATE_CALIB_MOVING_DOWN) {
        if (cmd.arg == 1) smCommandCalibDownSave();
        break;
      }
      smCommandMoveToFloor(cmd.arg);
      break;

    case CMD_MANUAL_UP:        smCommandManualUpHold();   break;
    case CMD_MANUAL_DOWN:      smCommandManualDownHold(); break;
    case CMD_MANUAL_STOP:      smCommandManualStop();     break;
    case CMD_STOP:             smCommandStop();           break;
    case CMD_CALIB:            smCommandStartCalib();     break;
    case CMD_CALIB_DOWN_START: smCommandCalibDownHold();  break;
    case CMD_CALIB_DOWN_SAVE:  smCommandCalibDownSave();  break;
    case CMD_CLEAR_ERROR:      smCommandClearError();     break;
    case CMD_HOME:             smCommandStartHoming();    break;
    case CMD_DISCOVER:         /* ответом служит очередной статус */ break;

    case CMD_NONE:
    default:
      LOG_W("[COMM] Unknown command %u", (unsigned)cmd.type);
      break;
  }
}

}  // namespace

bool commInit() {
  g_cmdQueue = xQueueCreate(8, sizeof(RemoteCommand));
  if (g_cmdQueue == nullptr) {
    LOG_E("[COMM] Queue allocation failed");
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  // Канал фиксируем явно: иначе стороны могут оказаться на разных каналах
  // и «молча» не слышать друг друга.
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  LOG_I("[COMM] Base MAC %02X:%02X:%02X:%02X:%02X:%02X, channel %u",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], ESPNOW_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    LOG_E("[COMM] esp_now_init failed");
    return false;
  }

  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onRecv);

  // Широковещательный peer нужен, чтобы отвечать пульту, который ещё
  // не знает MAC базы.
  addPeer(BROADCAST_MAC);

  LOG_I("[COMM] ESP-NOW ready");
  return true;
}

void commPoll() {
  if (g_pendingPeer) {
    g_pendingPeer = false;
    if (addPeer(g_pendingMac)) {
      memcpy(g_remoteMac, g_pendingMac, 6);
      g_haveRemoteMac = true;
      g_peerAdded     = true;
      LOG_I("[COMM] Remote %02X:%02X:%02X:%02X:%02X:%02X registered",
            g_remoteMac[0], g_remoteMac[1], g_remoteMac[2],
            g_remoteMac[3], g_remoteMac[4], g_remoteMac[5]);
    }
  }

  if (g_cmdQueue == nullptr) return;

  RemoteCommand cmd;
  while (xQueueReceive(g_cmdQueue, &cmd, 0) == pdTRUE) {
    handleCommand(cmd);
  }
}

void commSendStatusIfDue() {
  unsigned long now = millis();
  if (now - g_lastStatusMs < STATUS_PERIOD_MS) return;
  g_lastStatusMs = now;

  LiftStatus st;
  st.magic        = PROTO_MAGIC;
  st.version      = PROTO_VERSION;
  st.state        = (uint8_t)smGetState();
  st.currentFloor = smGetCurrentFloor();
  st.targetFloor  = smGetTargetFloor();
  st.direction    = smGetDirection();
  st.error        = smGetError();
  st.speedPercent = ioSpeedPercent();
  st.needCalib    = (smGetState() == STATE_NEED_CALIB) ? 1 : 0;
  st.uptimeMs     = now;

  // Пока пульт не найден, статус уходит широковещательно — по нему пульт
  // и узнаёт MAC базы. Это заменило захардкоженный адрес в прошивке пульта.
  const uint8_t *dst = g_peerAdded ? g_remoteMac : BROADCAST_MAC;
  esp_now_send(dst, (uint8_t *)&st, sizeof(st));
}

bool commHasPeer() {
  return g_peerAdded;
}
