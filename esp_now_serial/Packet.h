#pragma once

#include "Common.h"

namespace ESPNow
{
  static const size_t PACKET_MAX_SIZE = 255;
  
#pragma pack(push, 1)

  struct Packet
  {
    enum Type : uint8_t
    {
      DATA = 1U,
      DATA_RESPONCE = 2U,

      HANDSHAKE = 3U,
      HANDSHAKE_REPONCE = 4U
    };

    Packet(Type type)
    {
      header.type = type;
      header.magic_num = ESP_NOW_MAGIC_CONST;
    }

    struct Header
    {
      magic_num_t magic_num;

      Type type;

      peer_id_t from_peer;
      peer_id_t to_peer;

      packet_id_t packet_id;

    } header;

    static inline packet_id_t get_min_packet_id()
    {
      return 1U;
    }

    static inline packet_id_t get_max_packet_id()
    {
      switch(sizeof(packet_id_t))
      {
        case sizeof(uint8_t):
          return UINT8_MAX;

        case sizeof(uint16_t):
          return UINT16_MAX;
          
        case sizeof(uint32_t):
          return UINT32_MAX;
      }
    }

    union
    {
      struct Data
      {
        uint16_t length;
        
        uint8_t data[PACKET_MAX_SIZE - sizeof(header) - sizeof(length)];

      } data;

      struct Responce
      {
        packet_id_t packet_id;

      } responce;

      struct Handshake
      {
        packet_id_t packet_id;

      } responce;
    };

    size_t get_size();

    bool is_valid(size_t size);

    static inline size_t max_data_size()
    {
      return sizeof(data.data);
    }

    static size_t get_sizeof(Type type, size_t data_len = 0)
    {
      switch(type)
      {
        case HANDSHAKE:
        case HANDSHAKE_REPONCE:
          return sizeof(header) + sizeof(handshake);

        case DATA_RESPONCE:
          return sizeof(header) + sizeof(responce);

        case DATA:
          return sizeof(header) + sizeof(data) - 
            sizeof(data.data) + data_len;
      }
    }
  };

#pragma pack(pop)
}