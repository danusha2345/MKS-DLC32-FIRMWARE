#pragma once

#include <Arduino.h>
#include "ESPNowSerial.h"
#include "ESPNowSerialDevice.h"

#include <esp_now.h>
#include <esp_wifi.h>

#include <Arduino.h>
#include <freertos\task.h>
#include <freertos\ringbuf.h>

#ifdef ESPNOW_SERIAL_DEBUG

  #define ESPNOW_SERIAL_LOG(...) \
    if(log_func) { char buf[32]; snprintf(buf, sizeof(buf), __VA_ARGS__); log_func(buf); }

  #define ESPNOW_SERIAL_LOG_ERROR(...) \
    if(log_func) { char buf[32]; snprintf(buf, sizeof(buf), __VA_ARGS__); log_func(buf); }

  #define ESPNOW_SERIAL_ASSERT(...)

#else

  #define ESPNOW_SERIAL_LOG(...)

  #define ESPNOW_SERIAL_LOG_ERROR(...)

  #define ESPNOW_SERIAL_ASSERT(...)

#endif

#ifndef ESP_NOW_MAX_PEERS_COUNT
  #define ESP_NOW_MAX_PEERS_COUNT 1
#endif

#ifndef ESP_NOW_MAGIC_CONST
  #define ESP_NOW_MAGIC_CONST 0x9F21
#endif

namespace ESPNow
{
  typedef void (*ESPNowSerialCommonLogFunc)(const char* msg);
  

  typedef void PeerOnReceive(const uint8_t*, size_t);

  using peer_id_t = uint16_t;
  using packet_id_t = uint16_t;
  using magic_num_t = uint16_t;
  using data_size_t = uint16_t;
  using mac_address_t = uint8_t[6];

  static const device_id_t ANY_DEVICE = UINT16_MAX;
}