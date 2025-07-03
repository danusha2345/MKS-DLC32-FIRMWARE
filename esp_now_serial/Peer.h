#pragma once

#include "Common.h"

namespace ESPNow
{
  class Peer
  {
  private:
    
    friend class Device;

    bool is_connected;
    
    peer_id_t peer_id;
    
    uint16_t rx_buffer_size = 255;
    uint16_t tx_buffer_size = 255;

    RingbufHandle_t tx_buffer = NULL;
    RingbufHandle_t rx_buffer = NULL;

    SemaphoreHandle_t handshake_semaphore = NULL;

    ringbuf_type_t buffer_type = ringbuf_type_t::RINGBUF_TYPE_BYTEBUF;

    uint64_t data_sent_at = 0;
    uint16_t data_retry_count = 0;
      
    packet_id_t packet_id_counter = 0;
    packet_id_t next_data_packet_id = 0;
    packet_id_t current_data_packet_id = 0;

    uint8_t data_to_send[sizeof(Packet::data.data)];
    
    size_t data_size_to_send = 0;

    PeerOnReceive on_receive_callback = NULL;

    inline packet_id_t get_next_packet_id()
    {
      if(++packet_id_counter >= Packet::get_max_packet_id())
        packet_id_counter = Packet::get_min_packet_id();

      return packet_id_counter;
    }

    void on_connect();

    bool on_packet(Packet* packet);

    bool on_tx_queue(QueueSetMemberHandle_t* member);

    void send_packet(Packet* packet);

  public:
  
    bool is_connected();

    bool try_connect(uint32_t timeout_ms = UINT32_MAX, bool find_channel = true);

    bool send(uint8_t* data, size_t len, uint32_t timeout_ms = 0);
    size_t receive(uint8_t* data, size_t data_size, uint32_t timeout_ms = 0);
    
  };
}