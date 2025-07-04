#include "Device.h"
#include <Arduino.h>
#include <WiFi.h>

#include "esp_now.h"
#include "esp_wifi.h"

#include <freertos\task.h>
#include <freertos\ringbuf.h>

#define ESP_NOW_DEVICE_NO_PEER UINT8_MAX

using packet_id_t = uint16_t;
using magic_num_t = uint16_t;

uint16_t unsended_data_counter = 0;

peer_id_t current_peer_id = ESP_NOW_DEVICE_NO_PEER;

TaskHandle_t main_task = NULL;
RingbufHandle_t rx_packet_buf = NULL;
QueueSetHandle_t rx_tx_queue_set = NULL;

EspNowDeviceLog log_func = NULL;

esp_now_peer_info_t peer_info = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, {}, 0, wifi_interface_t::WIFI_IF_STA, false};

#if 1

  char _log_buffer[64];

  #define ESPNOW_SERIAL_LOG(...) \
    if(log_func) { snprintf(_log_buffer, sizeof(_log_buffer), __VA_ARGS__); log_func(_log_buffer); }

  #define ESPNOW_SERIAL_LOG_ERROR(...) \
    if(log_func) { snprintf(_log_buffer, sizeof(_log_buffer), __VA_ARGS__); log_func(_log_buffer); }

  #define ESPNOW_SERIAL_ASSERT(...)

    #define ESPNOW_SERIAL_ASSERT(ret_value, cond) \
    if (log_func && !(cond)) { \
      snprintf(_log_buffer, sizeof(_log_buffer), "%s %s:%i", #cond, __FILE__, __LINE__); \
      log_func(_log_buffer); \
    return ret_value; }

#else

  #define ESPNOW_SERIAL_LOG(...)

  #define ESPNOW_SERIAL_LOG_ERROR(...)

  #define ESPNOW_SERIAL_ASSERT(ret_value, cond) \
    if (!(cond)) {
      return (ret_value); \
    }

#endif

#pragma pack(push, 1)

  struct Packet
  {
    enum Type : uint8_t
    {
      Data = 1U,
      DataResponce = 2U,

      Handshake = 3U,
      HandshakeResponce = 4U
    };
    
    struct Header
    {
      magic_num_t magic_num;

      Type type;

      peer_id_t from_peer;
      peer_id_t to_peer;

    } header;

    union
    {
      struct
      {
        packet_id_t packet_id;

        uint16_t length;
        
        uint8_t data[ESP_NOW_DEVICE_MAX_PACKET_SIZE - 
          sizeof(header) - sizeof(length) - sizeof(packet_id)];

      } data;

      struct
      {
        uint8_t success;
        packet_id_t packet_id;

      } data_responce;

      struct
      {
        uint32_t wifi_channel;

      } handshake;

      struct
      {
        uint32_t wifi_channel;

      } handshake_responce;
    };

    bool validate(size_t size)
    {
        if(size < sizeof(header))
            return false;

        if(header.magic_num != ESP_NOW_DEVICE_MAGIC_CONST)
            return false;

        return true;
    }


    static inline size_t max_data_size()
    {
      return sizeof(data.data);
    }

    size_t get_size()
    {
      switch(header.type)
      {
        case Handshake:
            return sizeof(header) + sizeof(handshake);

        case HandshakeResponce:
          return sizeof(header) + sizeof(handshake_responce);

        case DataResponce:
          return sizeof(header) + sizeof(data_responce);

        case Data:
          return sizeof(header) + sizeof(data) - 
            sizeof(data.data) + data.length;
      }
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

    ESPNOW_SERIAL_LOG("send packet type=%i peer_id=%i", (int)type, (int)to_peer);
    
    ESPNOW_SERIAL_ASSERT(void(), esp_now_send(peer_info.peer_addr, (uint8_t*)packet, packet->get_size()) == ESP_OK);

    uint32_t ulNotifiedValue = 0;

    auto res = xTaskNotifyWait(0x00, 0x1 | 0x2, &ulNotifiedValue, pdMS_TO_TICKS(5));

    if(ulNotifiedValue & 0x2)
    {
        ESPNOW_SERIAL_LOG("esp_now_send error\n");
    }
}

struct Peer
{
    bool connected;

    peer_id_t peer_id;

    struct
    {
      size_t length;

      uint8_t data[sizeof(Packet::data.data)];

      packet_id_t sended_id;
      packet_id_t received_id;

      uint64_t send_time = 0;

    } data;
    
    inline packet_id_t next_id(packet_id_t id)
    {
      if(id == UINT16_MAX)
        return 0;

      return ++id;
    }

    size_t send_buffer_size;
    size_t receive_buffer_size;

    RingbufHandle_t send_buffer = NULL;
    RingbufHandle_t receive_buffer = NULL;

    SemaphoreHandle_t handshake_semaphore;

    inline bool is_connected()
    {
        return connected;
    }

    void setup()
    {
        if(send_buffer == NULL && receive_buffer == NULL)
        {
            ESPNOW_SERIAL_ASSERT(void(), send_buffer = xRingbufferCreate(send_buffer_size, RINGBUF_TYPE_NOSPLIT));
            ESPNOW_SERIAL_ASSERT(void(), receive_buffer = xRingbufferCreate(receive_buffer_size, RINGBUF_TYPE_NOSPLIT));
            
            ESPNOW_SERIAL_ASSERT(void(), xRingbufferAddToQueueSetRead(send_buffer, rx_tx_queue_set) == pdTRUE);
        }

        connected = true;
        data.sended_id = 0;
        data.received_id = 0;
    }

    void send_current_data()
    {
      new_packet.data.length = data.length;
      memcpy(new_packet.data.data, data.data, data.length);

      send_packet(Packet::Data, peer_id, &new_packet);

      data.send_time = micros();
    }

    void send_data_if_need()
    {
      if(data.send_time > 0)
        return;
      
      size_t size;

      auto data_ptr = xRingbufferReceive(send_buffer, &size, 0);

      if(data_ptr)
      {
        data.length = size;
        
        memcpy(data.data, data_ptr, size);

        vRingbufferReturnItem(send_buffer, data_ptr);

        send_current_data();

        unsended_data_counter++;
      }
    }

    void on_data_packet(Packet* packet)
    {
      if(data.received_id == 0 || (next_id(data.received_id) == packet->data.packet_id))
      {
          data.received_id = packet->data.packet_id;

          xRingbufferSend(receive_buffer, packet->data.data, packet->data.length, 0);

          new_packet.data_responce.packet_id = packet->data.packet_id;
          send_packet(Packet::DataResponce, peer_id, &new_packet);
      }
    }

    void on_responce_packet(Packet* packet)
    {
      if(data.sended_id == packet->data_responce.packet_id)
      {
        data.sended_id = next_id(data.sended_id);

        data.send_time = 0;

        unsended_data_counter--;

        send_data_if_need();
      }
    }

    void on_handshake_packet(Packet* packet)
    {
        setup();

        new_packet.handshake_responce.wifi_channel = WiFi.channel();

        send_packet(Packet::HandshakeResponce, peer_id, &new_packet);
    }

    void on_handshake_responce(Packet* packet)
    {
        setup();

        if(handshake_semaphore)
          xSemaphoreGive(handshake_semaphore);
    }

    void update()
    {
      if(data.send_time > 0)
      {
        if((micros() - data.send_time) > ESP_NOW_DEVICE_REPEAT_SEND_US)
          send_current_data();
      }
      else
      {
        send_data_if_need();
      }
    }

    bool try_connect(uint32_t timeout_ms, bool find_channel)
    {
        ESPNOW_SERIAL_LOG("try connect peer=%04x", (int)peer_id);

        auto start_time = millis();

        handshake_semaphore = xSemaphoreCreateBinary();

        do
        {
            if(find_channel)
            {
                for(auto i = 1U; i <= 13U; i++)
                {
                    ESPNOW_SERIAL_LOG("try channel=%i\n", (int)i);

                    ESPNOW_SERIAL_ASSERT(false, esp_wifi_set_channel(i, WIFI_SECOND_CHAN_NONE) == ESP_OK);

                    new_packet.handshake.wifi_channel = i;

                    send_packet(Packet::Handshake, peer_id, &new_packet);

                    if(xSemaphoreTake(handshake_semaphore, pdMS_TO_TICKS(50)) == pdTRUE)
                    {
                        break;
                    }
                }
            }
            else
            {
              new_packet.handshake.wifi_channel = WiFi.channel();
              send_packet(Packet::Handshake, peer_id, &new_packet);
                  
              if(xSemaphoreTake(handshake_semaphore, pdMS_TO_TICKS(30)) == pdTRUE)
              {
                  break;
              }
            }

        } while((millis() - start_time) <= timeout_ms);

        return is_connected();
    }

} peers[ESP_NOW_DEVICE_MAX_PEERS];

uint8_t used_peers = 0;

Peer* find_peer(peer_id_t peer_id)
{
    for(auto i = 0; i < used_peers; i++)
    {
        auto peer = &peers[i];

        if(peers[i].peer_id == peer_id)
        {
            return &peers[i];
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
      if(packet->header.to_peer != current_peer_id)
      {
          ESPNOW_SERIAL_LOG("new packet type=%i\n", (int)packet->header.type);
        
          xRingbufferSend(rx_packet_buf, data, len, 0);
      }
      else
      {
        ESPNOW_SERIAL_LOG("invalid packet, to_peer=%i\n", (int)packet->header.to_peer);
      }
    }
    else
    {
      ESPNOW_SERIAL_LOG("invalid packet, size=%i\n", (int)len);
    }
}

void main_task_loop(void*)
{
    size_t size;
    Packet responce;
        
    while(true)
    {
        auto member = xQueueSelectFromSet(rx_tx_queue_set, unsended_data_counter > 0 ? pdMS_TO_TICKS(5) : portMAX_DELAY);

        auto packet = (Packet*)xRingbufferReceive(rx_packet_buf, &size, 0);

        Peer* peer = find_peer(packet->header.to_peer);

        ESPNOW_SERIAL_LOG("process packet type=%i\n", (int)packet->header.type);

        if(peer)
        {
          switch(packet->header.type)
          {
              case Packet::Handshake:
                  peer->on_handshake_packet(packet);
                  break;

              case Packet::HandshakeResponce:
                  peer->on_handshake_responce(packet);
                  break;

              case Packet::Data:
                  peer->on_data_packet(packet);
                  break;

              case Packet::DataResponce:
                  peer->on_responce_packet(packet);
                  break;
          }

          vRingbufferReturnItem(rx_packet_buf, packet);
        }

        for(auto i = 0; i < used_peers; i++)
        {
            auto peer = &peers[i];

            if(peer->is_connected())
            {
                peer->send_data_if_need();
            }
        }
    }
}

bool EspNowDevice::add_peer(peer_id_t peer_id, size_t send_buffer_size, size_t receive_buffer_size)
{
    if(used_peers >= ESP_NOW_DEVICE_MAX_PEERS)
        return false;

    auto peer = &peers[used_peers++];

    peer->peer_id = peer_id;
    
    peer->send_buffer_size = send_buffer_size;
    peer->receive_buffer_size = receive_buffer_size;

    return true;
}

bool EspNowDevice::setup(peer_id_t peer_id) 
{
    if(current_peer_id != ESP_NOW_DEVICE_NO_PEER)
        return false;

    current_peer_id = peer_id;

    ESPNOW_SERIAL_ASSERT(false, esp_now_init() == ESP_OK);

    ESPNOW_SERIAL_ASSERT(false, rx_packet_buf = xRingbufferCreate(sizeof(Packet) * 2, RINGBUF_TYPE_NOSPLIT));

    ESPNOW_SERIAL_ASSERT(false, rx_tx_queue_set = xQueueCreateSet(ESP_NOW_DEVICE_MAX_PEERS + 1U));

    ESPNOW_SERIAL_ASSERT(false, xRingbufferAddToQueueSetRead(rx_packet_buf, rx_tx_queue_set) == pdTRUE);

    ESPNOW_SERIAL_ASSERT(false, esp_now_register_send_cb(EspNowOnDataSent) == ESP_OK);
    ESPNOW_SERIAL_ASSERT(false, esp_now_register_recv_cb(EspNowOnDataRecv) == ESP_OK);

    ESPNOW_SERIAL_ASSERT(false, esp_now_add_peer(&peer_info) == ESP_OK);

    ESPNOW_SERIAL_ASSERT(false, xTaskCreatePinnedToCore(main_task_loop, "espnow", 4096, NULL, 0, &main_task, 0) == pdPASS);

    ESPNOW_SERIAL_LOG_ERROR("ESPNowSerialCommon ch=%i\n", WiFi.channel());

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

bool EspNowDevice::send(peer_id_t peer_id, uint8_t* data, size_t length, uint32_t wait_ms)
{
  auto peer = find_peer(peer_id);

  if(peer && peer->is_connected())
  {
    return xRingbufferSend(peer->send_buffer, data, length, mills_to_tics(wait_ms)) == pdTRUE;
  }

  return false;
}

size_t EspNowDevice::receive(peer_id_t peer_id, uint8_t* data, size_t length, uint32_t wait_ms)
{
  auto peer = find_peer(peer_id);

  if(peer && peer->is_connected())
  {
    size_t size;

    auto data_ptr = xRingbufferReceiveUpTo(peer->receive_buffer, &size, mills_to_tics(wait_ms), length);

    if(data_ptr)
    {
      memcpy(data, data_ptr, size);

      return size;
    }
  }

  return false;
}

bool EspNowDevice::try_connect(peer_id_t peer_id, uint32_t timeout_ms, bool find_channel)
{
  auto peer = find_peer(peer_id);

  if(peer)
    return peer->try_connect(timeout_ms, find_channel);

  return false;
}

void EspNowDevice::set_log_callback(EspNowDeviceLog func)
{
  log_func = func;
}