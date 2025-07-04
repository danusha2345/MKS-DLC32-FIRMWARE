#pragma once

#include <Arduino.h>

using peer_id_t = uint8_t;

#ifndef ESP_NOW_DEVICE_DEFAULT_BUFFER_SIZE
    #define ESP_NOW_DEVICE_DEFAULT_BUFFER_SIZE 255
#endif

#ifndef ESP_NOW_DEVICE_MAGIC_CONST
    #define ESP_NOW_DEVICE_MAGIC_CONST 0xE91C
#endif

#ifndef ESP_NOW_DEVICE_MAX_PACKET_SIZE
    #define ESP_NOW_DEVICE_MAX_PACKET_SIZE 240
#endif

#ifndef ESP_NOW_DEVICE_REPEAT_SEND_US
    #define ESP_NOW_DEVICE_REPEAT_SEND_US 8000
#endif

#ifndef ESP_NOW_DEVICE_MAX_PEERS
    #define ESP_NOW_DEVICE_MAX_PEERS 1
#endif

typedef void (*EspNowDeviceLog)(const char* msg);
typedef void (*EspNowDataCallback)(uint8_t* data, size_t len);

class EspNowDevice
{
public:

    static void set_log_callback(EspNowDeviceLog func);

    static bool setup(peer_id_t peer_id);

    static bool add_peer(peer_id_t peer_id, 
        size_t send_buffer_size = ESP_NOW_DEVICE_DEFAULT_BUFFER_SIZE, 
            size_t receive_buffer_size = ESP_NOW_DEVICE_DEFAULT_BUFFER_SIZE);

    static bool try_connect(peer_id_t peer_id, uint32_t timeout_ms, bool find_channel);
    
    static bool send(peer_id_t peer_id, uint8_t* data, size_t length, uint32_t wait_ms);

    static size_t receive(peer_id_t peer_id, uint8_t* data, size_t length, uint32_t wait_ms);

    static uint64_t get_avg_ping(peer_id_t peer_id);
};