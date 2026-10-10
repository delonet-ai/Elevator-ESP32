#pragma once
#include <stdint.h>
#include <string.h>
#include <deque>
#include <vector>
#include <string>
using esp_err_t = int;
using esp_now_send_status_t = int;
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(mux) ((void)(mux))
#define portEXIT_CRITICAL(mux) ((void)(mux))
static const int ESP_OK=0, WIFI_IF_STA=0, WIFI_STA=1, ESP_MAC_WIFI_STA=0;
static const int WIFI_SECOND_CHAN_NONE=0, WIFI_COUNTRY_POLICY_MANUAL=0;
struct esp_now_peer_info_t { uint8_t peer_addr[6]; int channel,ifidx; bool encrypt; };
struct wifi_country_t { char cc[3]; uint8_t schan,nchan; int max_tx_power,policy; };
struct RadioPacket { std::vector<uint8_t> destination,bytes; };
struct FakeRadio {
  std::vector<RadioPacket> sent;
  std::vector<std::vector<uint8_t>> peers;
  int channel=1;
  void (*receive)(const uint8_t*, const uint8_t*, int)=nullptr;
};
extern FakeRadio radio;
inline bool esp_now_is_peer_exist(const uint8_t *mac) {
  for (const auto &peer : radio.peers) if (!memcmp(peer.data(),mac,6)) return true;
  return false;
}
inline int esp_now_add_peer(const esp_now_peer_info_t *peer) { radio.peers.emplace_back(peer->peer_addr,peer->peer_addr+6);return ESP_OK; }
inline int esp_now_init() { return ESP_OK; }
inline void esp_now_register_send_cb(void (*)(const uint8_t*,int)) {}
inline void esp_now_register_recv_cb(void (*fn)(const uint8_t*,const uint8_t*,int)) { radio.receive=fn; }
inline int esp_now_send(const uint8_t *mac,const uint8_t *data,size_t length) {
  radio.sent.push_back({{mac,mac+6},{data,data+length}});return ESP_OK;
}
inline void esp_read_mac(uint8_t *mac,int) { memset(mac,1,6); }
inline int esp_wifi_set_channel(uint8_t channel,int) { radio.channel=channel;return ESP_OK; }
inline void esp_wifi_set_country(const wifi_country_t*) {}
struct RadioWiFi {
  void mode(int) {} void setAutoReconnect(bool) {} void disconnect() {}
  int channel() { return radio.channel; }
  std::string macAddress() { return "01:01:01:01:01:01"; }
};
extern RadioWiFi WiFi;
struct FakeQueue { size_t capacity,size; std::deque<std::vector<uint8_t>> items; };
using QueueHandle_t=FakeQueue*;
static const int pdTRUE=1;
inline QueueHandle_t xQueueCreate(size_t capacity,size_t size) { return new FakeQueue{capacity,size,{}}; }
inline int xQueueSend(QueueHandle_t queue,const void *data,int) {
  if(queue->items.size()==queue->capacity)return 0;
  const auto *bytes=static_cast<const uint8_t*>(data);
  queue->items.emplace_back(bytes,bytes+queue->size);return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t queue,void *out,int) {
  if(queue->items.empty())return 0;
  memcpy(out,queue->items.front().data(),queue->size);queue->items.pop_front();return pdTRUE;
}
