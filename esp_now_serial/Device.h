#pragma once

#include "Packet.h"
#include "Peer.h"

#ifndef ESP_NOW_MAX_DEVICE_COUNT
  #define ESP_NOW_MAX_DEVICE_COUNT 1
#endif

namespace ESPNow
{
  class Device
  {
  private:

    friend class Peer;

    static uint8_t used_peers;
    
    static Peer* peers[ESP_NOW_MAX_PEERS_COUNT];

    static TaskHandle_t task;

    static peer_id_t current_peer_id;

    static RingbufHandle_t rx_ring_buf;
    static QueueSetHandle_t rx_tx_queue;
    
    static ESPNowSerialCommonLogFunc log_func;
    
    static Packet new_packet;

    static bool setup_done;

    static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
    static void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int len);

    static void on_new_packet(Packet* packet)
    {
      switch(packet->header.type)
      {
        case Packet::DATA:
        case Packet::DATA_RESPONCE:

        case Packet::HANDSHAKE:
        case Packet::HANDSHAKE_REPONCE:
        {
          auto device = find_device(packet->header.from_device);

          if(device)
            device->on_packet(packet);

          break;
        }
      }
    }

    static void loop(void* arg);

    static Packet* begin_send(Packet::Type type, data_size_t size = 0);

    static void end_send(Packet* packet);

  public:

    static bool setup(peer_id_t peer_id);

    static Peer* find_peer(peer_id_t peer_id);

    static bool register_peer(Peer* peer);
  };
}