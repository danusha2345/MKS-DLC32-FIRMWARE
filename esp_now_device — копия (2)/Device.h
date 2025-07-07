#pragma once

#include <Arduino.h>
#include <freertos\ringbuf.h>

using peer_id_t = uint8_t;

#ifndef ESP_NOW_DEVICE_MAGIC_CONST
    #define ESP_NOW_DEVICE_MAGIC_CONST 0xE91C
#endif

#ifndef ESP_NOW_DEVICE_MAX_PACKET_SIZE
    #define ESP_NOW_DEVICE_MAX_PACKET_SIZE 235
#endif

#ifndef ESP_NOW_DEVICE_DEFAULT_BUFFER_SIZE
    #define ESP_NOW_DEVICE_DEFAULT_BUFFER_SIZE 512
#endif

#ifndef ESP_NOW_DEVICE_REPEAT_SEND_US
    #define ESP_NOW_DEVICE_REPEAT_SEND_US 10000
#endif

#ifndef ESP_NOW_DEVICE_MAX_PEERS
    #define ESP_NOW_DEVICE_MAX_PEERS 1
#endif

#define ESP_NOW_DEVICE_NO_PEER UINT8_MAX

namespace EspNow
{
    typedef void (*EspNowDeviceLog)(const char* msg);
    typedef void (*EspNowDataCallback)(uint8_t* data, size_t len);

    struct Packet;

    using packet_id_t = uint16_t;
    using magic_num_t = uint16_t;

    #pragma pack(push, 1)

        struct PacketHeader
        {
            magic_num_t magic_num;

            uint8_t type;

            peer_id_t from_peer;
            peer_id_t to_peer;

            uint8_t wifi_channel;

        };

        struct PacketData
        {
            packet_id_t packet_id;

            uint16_t length;
            
            uint8_t data[ESP_NOW_DEVICE_MAX_PACKET_SIZE - 
                sizeof(PacketHeader) - sizeof(length) - sizeof(packet_id)];
        };

    #pragma pack(pop)

    class Peer
    {
    public:

        bool _connected = false;
        bool _initialized = false;

        int32_t _wifi_channel = 0;

        peer_id_t _peer_id = ESP_NOW_DEVICE_NO_PEER;

        struct
        {
            size_t length;

            uint8_t data[sizeof(PacketData::data)];

            packet_id_t sended_id = 0;
            packet_id_t received_id = 0;

            uint64_t send_time = 0;
            uint64_t begin_send_time = 0;

            uint64_t avg_ping = 0;
            uint32_t resended_count = 0;

        } _data;

        size_t _send_buffer_size = ESP_NOW_DEVICE_DEFAULT_BUFFER_SIZE;
        size_t _receive_buffer_size = ESP_NOW_DEVICE_DEFAULT_BUFFER_SIZE;

        RingbufHandle_t _send_buffer = NULL;
        RingbufHandle_t _receive_buffer = NULL;

        SemaphoreHandle_t _handshake_semaphore = NULL;

        EspNowDataCallback _data_callback;

        void _initialize();
        void _on_connect();

        void _send_current_data();

        void _send_data_if_need();

        void _on_data_packet(Packet* packet);

        void _on_responce_packet(Packet* packet);

        bool _on_packet_begin(Packet* packet);

        void _update();

    public:

        Peer(peer_id_t id) : _peer_id(id)
        {
            
        }

        inline bool is_connected()
        {
            return _connected;
        }

        bool try_connect(uint32_t timeout_ms, bool find_channel);

        bool send(uint8_t* data, size_t length, uint32_t wait_ms);

        size_t receive(uint8_t* data, size_t length, uint32_t wait_ms);

        inline uint64_t get_avg_ping()
        {
            return _data.avg_ping;
        }

        void set_data_callback(EspNowDataCallback data_callback)
        {
            _data_callback = data_callback;
        }
    };

    class Device
    {
    public:

        static void set_log_callback(EspNowDeviceLog func);

        static bool setup(peer_id_t peer_id);

        static bool add_peer(Peer* peer);
    };
};