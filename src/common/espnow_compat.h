#pragma once
//
// Сигнатуры колбэков ESP-NOW разъехались между Arduino-core 2.x (IDF 4.x)
// и 3.x (IDF 5.x). Шим позволяет собирать один и тот же код обеими версиями.
//
#include <esp_now.h>

// esp_read_mac() переехал в esp_mac.h только в IDF 5.x.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#include <esp_mac.h>
#else
#include <esp_system.h>
#endif

#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
#define ESPNOW_SEND_CB(name) \
  void name(const wifi_tx_info_t *info, esp_now_send_status_t status)
#define ESPNOW_RECV_CB(name) \
  void name(const esp_now_recv_info_t *info, const uint8_t *data, int len)
// MAC отправителя внутри ESPNOW_RECV_CB
#define ESPNOW_RECV_SRC_MAC (info->src_addr)
#else
#define ESPNOW_SEND_CB(name) \
  void name(const uint8_t *mac_addr, esp_now_send_status_t status)
#define ESPNOW_RECV_CB(name) \
  void name(const uint8_t *mac_addr, const uint8_t *data, int len)
#define ESPNOW_RECV_SRC_MAC (mac_addr)
#endif
