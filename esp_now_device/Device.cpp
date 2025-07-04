#include "Device.h"
#include <Arduino.h>
#include <WiFi.h>

#include "esp_now.h"
#include "esp_wifi.h"

#include <freertos\task.h>

namespace EspNow
{
  peer_id_t current_peer_id = ESP_NOW_DEVICE_NO_PEER;

  TaskHandle_t main_task = NULL;
  RingbufHandle_t rx_packet_buf = NULL;
  QueueSetHandle_t rx_tx_queue_set = NULL;

  int32_t current_wifi_channel = 0;

  EspNowDeviceLog log_func = NULL;

  esp_now_peer_info_t peer_info = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, {}, 0, wifi_interface_t::WIFI_IF_STA, false};

  const packet_id_t FIRST_PACKET_ID = 1U;
  const packet_id_t HANSHAKE_PACKET_ID = UINT8_MAX;

  inline packet_id_t get_next_packet_id(packet_id_t id)
  {
    if(id == (UINT8_MAX - 1U))
      return FIRST_PACKET_ID;
    
    return ++id;
  }

  #define AVG_FILTER(value, avg, level) (((((level) - 1) * (avg)) + (value)) / (level))

  #if 0

    char _log_buffer[64];

    #define ESPNOW_SERIAL_LOG(...) \
      if(log_func) { snprintf(_log_buffer, sizeof(_log_buffer), __VA_ARGS__); log_func(_log_buffer); }

    #define ESPNOW_SERIAL_LOG_ERROR(...) \
      if(log_func) { snprintf(_log_buffer, sizeof(_log_buffer), __VA_ARGS__); log_func(_log_buffer); }

    #define ESPNOW_SERIAL_ASSERT(...)

      #define ESPNOW_SERIAL_ASSERT(ret_value, cond) \
      if (!(cond)) { \
        if(log_func) { snprintf(_log_buffer, sizeof(_log_buffer), "%s %s:%i", #cond, __FILE__, __LINE__); \
        log_func(_log_buffer); } \
      return ret_value; }

  #else

    char _log_buffer[64];

    #define ESPNOW_SERIAL_LOG(...)

    #define ESPNOW_SERIAL_LOG_ERROR(...) \
        if(log_func) { snprintf(_log_buffer, sizeof(_log_buffer), __VA_ARGS__); log_func(_log_buffer); }

    #define ESPNOW_SERIAL_ASSERT(ret_value, cond) if (!(cond)) return (ret_value);

  #endif

  #pragma pack(push, 1)

    struct Packet
    {
      enum Type : uint8_t
      {
        Data = 1U,
        DataResponce = 2U
      };
      
      PacketHeader header;

      union
      {
        PacketData data;

        struct
        {
          packet_id_t packet_id;

        } data_responce;
      };

      bool validate(size_t size)
      {
          if(size < sizeof(header))
              return false;

          if(header.magic_num != ESP_NOW_DEVICE_MAGIC_CONST)
              return false;

          return true;
      }

      size_t get_size()
      {
        switch(header.type)
        {
          case DataResponce:
            return sizeof(header) + sizeof(data_responce);

          case Data:
            return sizeof(header) + sizeof(data) - 
              sizeof(data.data) + data.length;
        }

        return sizeof(header);
      }
    };

  #pragma pack(pop)

  Packet new_packet;

  void send_packet(Packet::Type type, peer_id_t to_peer, Packet* packet)
  {
      packet->header.magic_num = ESP_NOW_DEVICE_MAGIC_CONST;

      packet->header.type = type;
      packet->header.to_peer = to_peer;
      packet->header.from_peer = current_peer_id;
      packet->header.wifi_channel = current_wifi_channel;

      ESPNOW_SERIAL_LOG("send packet type=%i peer_id=%i\n", (int)type, (int)to_peer);
      
      ESPNOW_SERIAL_ASSERT(void(), esp_now_send(peer_info.peer_addr, (uint8_t*)packet, packet->get_size()) == ESP_OK);

      uint32_t ulNotifiedValue = 0;

      auto res = xTaskNotifyWait(0x00, 0x1 | 0x2, &ulNotifiedValue, pdMS_TO_TICKS(5));

      if(ulNotifiedValue & 0x2)
      {
          ESPNOW_SERIAL_LOG("esp_now_send error\n");
      }
  }

  inline BaseType_t xRingbufferSendOverride(RingbufHandle_t xRingbuffer,const void *pvItem, size_t xItemSize, TickType_t xTicksToWait)
  {
    size_t free_size;

    if((free_size = xRingbufferGetCurFreeSize(xRingbuffer)) < xItemSize)
    {
      size_t size = 0;
      auto data = xRingbufferReceiveUpTo(xRingbuffer, &size, 0, xItemSize - free_size);
      vRingbufferReturnItem(xRingbuffer, data);
    }

    return xRingbufferSend(xRingbuffer, pvItem, xItemSize, xTicksToWait);
  }

  void Peer::_initialize()
  {
      if(!_initialized)
      {
          if(!_data_callback)
            ESPNOW_SERIAL_ASSERT(void(), _receive_buffer = xRingbufferCreate(_receive_buffer_size, RINGBUF_TYPE_BYTEBUF));
       
          ESPNOW_SERIAL_ASSERT(void(), _send_buffer = xRingbufferCreate(_send_buffer_size, RINGBUF_TYPE_BYTEBUF));
          ESPNOW_SERIAL_ASSERT(void(), xRingbufferAddToQueueSetRead(_send_buffer, rx_tx_queue_set) == pdTRUE);

          _data.sended_id = FIRST_PACKET_ID;
          _data.received_id = FIRST_PACKET_ID;

          _initialized = true;
      }

      ESPNOW_SERIAL_LOG("setup peer_id=%i\n", (int)peer_id);
  }

  void Peer::_on_connect()
  {
    if(!_connected)
    {
      _connected = true;

      _data.sended_id = FIRST_PACKET_ID;
      _data.received_id = FIRST_PACKET_ID;
    }
  }

  void Peer::_send_current_data()
  {
    new_packet.data.length = _data.length;
    new_packet.data.packet_id = _data.sended_id;

    memcpy(new_packet.data.data, _data.data, _data.length);

    send_packet(Packet::Data, _peer_id, &new_packet);

    _data.send_time = micros();
  }

  void Peer::_send_data_if_need()
  {
    if(_data.send_time > 0)
      return;
    
    size_t size;

    auto data_ptr = xRingbufferReceiveUpTo(_send_buffer, &size, 0, sizeof(_data.data));

    if(data_ptr)
    {
      _data.length = size;
      
      memcpy(_data.data, data_ptr, size);

      vRingbufferReturnItem(_send_buffer, data_ptr);

      _data.begin_send_time = micros();

      _send_current_data();
    }
  }

  void Peer::_on_data_packet(Packet* packet)
  {
    bool is_first_packet = _data.received_id == FIRST_PACKET_ID || packet->data.packet_id == FIRST_PACKET_ID;

    if(is_first_packet || (get_next_packet_id(_data.received_id) == packet->data.packet_id))
    {
        _data.received_id = packet->data.packet_id;

        ESPNOW_SERIAL_LOG("on_data_packet len=%i\n", (int)packet->data.length);

        if(_data_callback)
        {
          _data_callback(packet->data.data, packet->data.length);
        }
        else
        {
          xRingbufferSendOverride(_receive_buffer, packet->data.data, packet->data.length, 0);
        }

        new_packet.data_responce.packet_id = packet->data.packet_id;
        send_packet(Packet::DataResponce, _peer_id, &new_packet);
    }
    else
    {
      new_packet.data_responce.packet_id = packet->data.packet_id;
      send_packet(Packet::DataResponce, _peer_id, &new_packet);
    }
  }

  void Peer::_on_responce_packet(Packet* packet)
  {
    if(_data.sended_id == packet->data_responce.packet_id)
    {
      auto ping = (micros() - _data.begin_send_time);

      if(_data.avg_ping == 0)
      {
        _data.avg_ping = ping;
      }
      else
      {
        _data.avg_ping = AVG_FILTER(ping, _data.avg_ping, 3UL);
      }

      _data.sended_id = get_next_packet_id(_data.sended_id);

      _data.send_time = 0;

      _send_data_if_need();
    }
  }

  bool Peer::_on_packet_begin(Packet* packet)
  {
    if(packet->header.type == Packet::Data && packet->data.packet_id == HANSHAKE_PACKET_ID)
    {
      _on_connect();

      new_packet.data_responce.packet_id = HANSHAKE_PACKET_ID;
      send_packet(Packet::DataResponce, _peer_id, &new_packet);

      return false;
    }
    else if(packet->header.type == Packet::DataResponce && packet->data_responce.packet_id == HANSHAKE_PACKET_ID)
    {
      _on_connect();

      if(_handshake_semaphore)
      {
        xSemaphoreGive(_handshake_semaphore);
        _wifi_channel = current_wifi_channel;

        ESPNOW_SERIAL_LOG("xSemaphoreGive peer_id=%i\n", (int)peer_id);
      }

      return false;
    }
    else if(!_connected)
    {
      _on_connect();
    }

    return true;
  }

  void Peer::_update()
  {
    if(_data.send_time > 0)
    {
      if((micros() - _data.send_time) > ESP_NOW_DEVICE_REPEAT_SEND_US)
      {
        _send_current_data();

        _data.resended_count++;
      }
    }
    else
    {
      _send_data_if_need();
    }
  }

  bool Peer::try_connect(uint32_t timeout_ms, bool find_channel)
  {
      ESPNOW_SERIAL_ASSERT(false, _handshake_semaphore == NULL);

      ESPNOW_SERIAL_LOG("try connect peer=%i\n", (int)peer_id);

      auto start_time = millis();

      _handshake_semaphore = xSemaphoreCreateBinary();

      _wifi_channel = 0;

      do
      {
          if(find_channel)
          {
              for(auto i = 1U; i < 14U; i++)
              {

                  ESPNOW_SERIAL_LOG("try channel=%i\n", (int)i);

                  ESPNOW_SERIAL_ASSERT(false, esp_now_del_peer(peer_info.peer_addr) == ESP_OK);

                  ESPNOW_SERIAL_ASSERT(false, esp_wifi_set_channel(i, WIFI_SECOND_CHAN_NONE) == ESP_OK);

                  ESPNOW_SERIAL_ASSERT(false, esp_now_add_peer(&peer_info) == ESP_OK);

                  current_wifi_channel = i;

                  delay(30);

                  new_packet.data.length = 0;

                  new_packet.data.packet_id = HANSHAKE_PACKET_ID;

                  send_packet(Packet::Data, _peer_id, &new_packet);

                  if(xSemaphoreTake(_handshake_semaphore, pdMS_TO_TICKS(100)) == pdTRUE)
                  {
                    break;
                  }
              }
          }
          else
          {
            new_packet.data.length = 0;

            new_packet.data.packet_id = HANSHAKE_PACKET_ID;

            send_packet(Packet::Data, _peer_id, &new_packet);
            
            if(xSemaphoreTake(_handshake_semaphore, pdMS_TO_TICKS(100)) == pdTRUE)
            {
                break;
            }
          }

          if(_wifi_channel)
          {
            current_wifi_channel = _wifi_channel;

            break;
          }

      } while((millis() - start_time) <= timeout_ms);

      vSemaphoreDelete(_handshake_semaphore);
      _handshake_semaphore = NULL;

      return is_connected();
  }

  Peer* peers[ESP_NOW_DEVICE_MAX_PEERS];

  uint8_t used_peers = 0;

  Peer* find_peer(peer_id_t peer_id)
  {
      for(auto i = 0; i < used_peers; i++)
      {
          if(peers[i]->_peer_id == peer_id)
          {
              return peers[i];
          }
      }

      return NULL;
  }

  void EspNowOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
  {
      xTaskNotify(main_task, status == ESP_NOW_SEND_SUCCESS ? 0x1 : 0x2, eSetBits);
  }

  void EspNowOnDataRecv(const uint8_t * mac_addr, const uint8_t *data, int len) 
  {
      auto packet = (Packet*)data;

      if(packet->validate(len))
      {
        if(packet->header.to_peer == current_peer_id)
        {
            ESPNOW_SERIAL_LOG("new packet type=%i\n", (int)packet->header.type);
          
            xRingbufferSend(rx_packet_buf, data, len, 0);
        }
        else
        {
          ESPNOW_SERIAL_LOG("invalid packet, to_peer=%i, type=%i, this_peer=%i\n", 
            (int)packet->header.to_peer, (int)packet->header.type, (int)current_peer_id);
        }
      }
      else
      {
        ESPNOW_SERIAL_LOG("invalid packet, size=%i\n", (int)len);
      }
  }

  bool setup_internal()
  {
    ESPNOW_SERIAL_ASSERT(false, esp_now_init() == ESP_OK);

    ESPNOW_SERIAL_ASSERT(false, rx_packet_buf = xRingbufferCreate(sizeof(Packet) * 2, RINGBUF_TYPE_NOSPLIT));

    ESPNOW_SERIAL_ASSERT(false, rx_tx_queue_set = xQueueCreateSet(ESP_NOW_DEVICE_MAX_PEERS * 3));

    ESPNOW_SERIAL_ASSERT(false, xRingbufferAddToQueueSetRead(rx_packet_buf, rx_tx_queue_set) == pdTRUE);

    ESPNOW_SERIAL_ASSERT(false, esp_now_add_peer(&peer_info) == ESP_OK);

    ESPNOW_SERIAL_ASSERT(false, esp_now_register_send_cb(EspNowOnDataSent) == ESP_OK);
    ESPNOW_SERIAL_ASSERT(false, esp_now_register_recv_cb(EspNowOnDataRecv) == ESP_OK);

    current_wifi_channel = WiFi.channel();

    return true;
  }

  bool has_unsend_data()
  {
    for(auto i = 0; i < used_peers; i++)
    {
        auto peer = peers[i];

        if(peer->is_connected() && peer->_data.send_time > 0)
        {
            return true;
        }
    }

    return false;
  }

  void main_task_loop(void*)
  {
      size_t size;
      Packet responce;

      while(true)
      {
          auto member = xQueueSelectFromSet(rx_tx_queue_set, 
            has_unsend_data() ? pdMS_TO_TICKS(3) : portMAX_DELAY);

          auto packet = (Packet*)xRingbufferReceive(rx_packet_buf, &size, 0);

          if(packet)
          {
            Peer* peer = find_peer(packet->header.from_peer);

            ESPNOW_SERIAL_LOG("process packet type=%i peer=%i, found: %s\n", 
              (int)packet->header.type, (int)packet->header.from_peer,
                peer ? "true" : "false");
            
            if(peer)
            {
              if(peer->_on_packet_begin(packet))
              {
                switch(packet->header.type)
                {
                    case Packet::Data:
                        peer->_on_data_packet(packet);
                        break;

                    case Packet::DataResponce:
                        peer->_on_responce_packet(packet);
                        break;
                }
              }
            }

            vRingbufferReturnItem(rx_packet_buf, packet);
          }

          for(auto i = 0; i < used_peers; i++)
          {
              auto peer = peers[i];

              if(peer->is_connected())
              {
                  peer->_update();
              }
          }
      }
  }

  bool Device::add_peer(Peer* peer)
  {
      if(current_peer_id == ESP_NOW_DEVICE_NO_PEER)
        return false;
      
      if(used_peers >= ESP_NOW_DEVICE_MAX_PEERS)
          return false;

      peer->_initialize();

      peers[used_peers++] = peer;

      return true;
  }

  bool Device::setup(peer_id_t peer_id) 
  {
      if(current_peer_id != ESP_NOW_DEVICE_NO_PEER)
          return false;

      current_peer_id = peer_id;

      if(!setup_internal())
        return false;

      ESPNOW_SERIAL_ASSERT(false, xTaskCreatePinnedToCore(main_task_loop, "espnow", 4096, NULL, 0, &main_task, 0) == pdPASS);

      ESPNOW_SERIAL_LOG("ESPNowSerialCommon ch=%i\n", WiFi.channel());

      return true;
  }

  inline TickType_t mills_to_tics(uint32_t time_ms)
  {
    if(time_ms == 0)
      return 0;

    if(time_ms == UINT32_MAX)
      return portMAX_DELAY;

    return pdMS_TO_TICKS(time_ms);
  }

  bool Peer::send(uint8_t* data, size_t length, uint32_t wait_ms)
  {
    if(is_connected())
    {
      return xRingbufferSendOverride(_send_buffer, data, length, mills_to_tics(wait_ms)) == pdTRUE;
    }
    else
    {
      ESPNOW_SERIAL_LOG("send peer_id=%i not found or not connected\n", (int)peer_id);
    }

    return false;
  }

  size_t Peer::receive(uint8_t* data, size_t length, uint32_t wait_ms)
  {
    if(_data_callback)
      return 0;
    
    if(is_connected())
    {
      size_t size;

      auto data_ptr = xRingbufferReceiveUpTo(_receive_buffer, &size, mills_to_tics(wait_ms), length);

      if(data_ptr)
      {
        memcpy(data, data_ptr, size);

        vRingbufferReturnItem(_receive_buffer, data_ptr);

        return size;
      }
    }
    else
    {
      ESPNOW_SERIAL_LOG("receive peer_id=%i not found or not connected\n", (int)peer_id);
    }

    return false;
  }

  void Device::set_log_callback(EspNowDeviceLog func)
  {
    log_func = func;
  }
}