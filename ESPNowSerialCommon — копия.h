#pragma once

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

#else

  #define ESPNOW_SERIAL_LOG(...)

  #define ESPNOW_SERIAL_LOG_ERROR(...)

#endif

typedef void (*ESPNowSerialCommonLogFunc)(const char* msg);

extern "C"
{
  esp_err_t esp_wifi_config_espnow_rate(wifi_interface_t ifx, wifi_phy_rate_t rate);
}

class ESPNowSerialCommon
{
protected:

    using packet_id_t = uint16_t;
    using magic_num_t = uint16_t;
    using mac_address_t = uint8_t[6];

    static const uint16_t NOTIFY_RX = 0x1;
    static const uint16_t NOTIFY_TX = 0x2;

    static const uint16_t NOTIFY_SENT_SUCCESS = 0x4;
    static const uint16_t NOTIFY_SENT_ERROR = 0x8;

    static const magic_num_t MAGIC_NUM = 0x1EC9;

    static const size_t PACKET_MAX_SIZE = 250;

    static const uint64_t TX_RETRY_TIME_US = 20000;

    static TaskHandle_t task;

    static RingbufHandle_t rx_ring_buf;
    static RingbufHandle_t tx_ring_buf;
    static RingbufHandle_t serial_ring_buf;
    static SemaphoreHandle_t handshake_sem;

    static ESPNowSerialCommonLogFunc log_func;

    static uint8_t peer_addr[6];
    
    static bool is_broadcast_addr;

    static esp_now_peer_info_t peer_info;

    static bool setup_done;

    #pragma pack(push, 1)

    struct PACKET
    {
      enum PACKET_TYPE : uint16_t
      {
        DATA = 1U,
        DATA_RESPONCE = 2U,

        HANDSHAKE = 3U,
        HANDSHAKE_REPONCE = 4U
      };

      struct
      {
        magic_num_t magic_num;

        PACKET_TYPE type;

        packet_id_t packet_id;

      } header;

      union
      {
        struct
        {
          packet_id_t next_packet_id;

          uint16_t length;

          uint8_t data[PACKET_MAX_SIZE - sizeof(header) - sizeof(length) - sizeof(next_packet_id)];

        } data;

        struct
        {
          uint8_t mac[6];

        } handshake;

        struct
        {
          packet_id_t packet_id;

        } responce;
      };

      size_t get_size()
      {
        switch(header.type)
        {
          case DATA_RESPONCE:
            return sizeof(header) + sizeof(responce);

          case HANDSHAKE:
          case HANDSHAKE_REPONCE:
            return sizeof(header) + sizeof(handshake);

          case DATA:
            return sizeof(header) + sizeof(data.length) + data.length;

          default:
            return 0;
        }
      }

      bool is_valid(size_t size)
      {
        if(size < sizeof(header))
          return false;

        if(header.magic_num != MAGIC_NUM)
          return false;

        return true;
      }
    };

    struct PACKET_BUF
    {
      mac_address_t mac;
      PACKET packet;
    };

    #pragma pack(pop)

    static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
    {
      xTaskNotify(task, status == ESP_NOW_SEND_SUCCESS ? NOTIFY_SENT_SUCCESS : NOTIFY_SENT_ERROR, eSetBits);
    }

    static void OnDataRecv(const uint8_t * mac, const uint8_t *incoming_data, int data_len) 
    {
        auto packet = (PACKET*)incoming_data;

        if(!packet->is_valid(data_len))
        {
          ESPNOW_SERIAL_LOG("OnDataRecv invalid packet\n");
          return;
        }

        PACKET_BUF* buf_ptr;

        if(xRingbufferSendAcquire(rx_ring_buf, (void**)&buf_ptr, sizeof(mac_address_t) + data_len, 0) == pdTRUE)
        {
          memcpy(buf_ptr->mac, mac, sizeof(mac_address_t));
          memcpy(&buf_ptr->packet, packet, data_len);
          
          xRingbufferSendComplete(rx_ring_buf, buf_ptr);
        }
        else
        {
           ESPNOW_SERIAL_LOG("ringbuf send err\n");
        }

        xTaskNotify(task, NOTIFY_RX, eSetBits);
    }

    static void esp_now_send_packet(PACKET* packet)
    {
      esp_err_t error;
      
      if((error = esp_now_send(peer_addr, (uint8_t*)packet, packet->get_size())) != ESP_OK)
      {
        ESPNOW_SERIAL_LOG("esp_now_send=%i\n", (int)error);
      }

      uint32_t ulNotifiedValue = 0;

      auto res = xTaskNotifyWait(0x00, NOTIFY_SENT_SUCCESS | NOTIFY_SENT_ERROR, &ulNotifiedValue, pdMS_TO_TICKS(5));

      if(ulNotifiedValue & NOTIFY_SENT_ERROR)
      {
        ESPNOW_SERIAL_LOG("esp_now_send error\n");
      }
    }

    static void on_new_packet(PACKET_BUF* packet_ptr)
    {
        static PACKET responce;

        static packet_id_t next_packet_id = 0;
        static uint16_t mistmatch_packets_count = 0;

        PACKET* packet = &packet_ptr->packet;

        if(packet->header.type == PACKET::DATA)
        {
          if(next_packet_id == 0 || packet->header.packet_id == next_packet_id)
          {
            next_packet_id = packet->data.next_packet_id;
            
            xRingbufferSend(serial_ring_buf, packet->data.data, packet->data.length, 0);
          }
          else
          {
            if(mistmatch_packets_count++ > 8)
            {
              next_packet_id = 0;
            }
          }

          responce = {{MAGIC_NUM, PACKET::DATA_RESPONCE}};
          responce.responce.packet_id = packet->data.packet_id;
          
          esp_now_send_packet(&responce);     
        }
        else if(packet->header.type == PACKET::HANDSHAKE)
        {
          responce = {{MAGIC_NUM, PACKET::HANDSHAKE_REPONCE}};

          WiFi.macAddress(responce.handshake.mac);

          esp_now_send_packet(&responce);

          next_packet_id = mistmatch_packets_count = tx_first_sent_at = tx_sent_at = 0;
        }
        else if(packet->header.type == PACKET::HANDSHAKE_REPONCE)
        {
          next_packet_id = mistmatch_packets_count = tx_first_sent_at = tx_sent_at = 0;

          if(handshake_sem)
          {
            xSemaphoreGive(handshake_sem);
          }
        }
        else if(packet->header.type == PACKET::DATA_RESPONCE)
        {
          if(packet->responce.packet_id == data_packet.header.packet_id)
          {
            //ESPNOW_SERIAL_LOG("PING %in", (uint32_t)(micros() - tx_sent_at));
            tx_sent_at = 0;
            retry_count = 0;
          }
        }
    }

    static packet_id_t get_next_packet_id()
    {
      static packet_id_t tx_packet_id = 0;

      tx_packet_id++;

      if(tx_packet_id == UINT16_MAX)
        tx_packet_id = 1;

      return tx_packet_id;
    }

    static void loop(void* arg)
    {
        static void* received_serial_data = NULL;
        
        static uint16_t retry_count = 0;

        static uint64_t tx_sent_at = 0;
        static uint64_t tx_first_sent_at = 0;

        static packet_id_t next_tx_packet_id = get_next_packet_id();

        esp_err_t err = ESP_OK;

        PACKET data_packet = {{MAGIC_NUM, PACKET::DATA}};

        for(;;)
        {
            size_t size;
            uint32_t ulNotifiedValue;

            xTaskNotifyWait(0x00, ULONG_MAX, &ulNotifiedValue, pdMS_TO_TICKS(1U));

            if(tx_sent_at > 0 && (micros() - tx_sent_at) > TX_RETRY_TIME_US)
            {
              tx_sent_at = micros();

              esp_now_send_packet(&data_packet);

              retry_count++;
            }

            if(tx_sent_at == 0)
            {
              received_serial_data = xRingbufferReceiveUpTo(tx_ring_buf, &size, 0, sizeof(data_packet.data.data));

              if(received_serial_data)
              {
                  data_packet.header.packet_id = next_tx_packet_id;
                  next_tx_packet_id = get_next_packet_id();

                  data_packet.data.length = size;
                  memcpy(data_packet.data.data, received_serial_data, size);
                  
                  esp_now_send_packet(&data_packet);

                  tx_first_sent_at = tx_sent_at = micros();

                  vRingbufferReturnItem(tx_ring_buf, received_serial_data);
              }
            }

            auto packet_ptr = (PACKET_BUF*)xRingbufferReceive(rx_ring_buf, &size, 0);

            if(packet_ptr)
            {
              on_new_packet(packet_ptr);
              
              vRingbufferReturnItem(rx_ring_buf, packet_ptr);
            }

            delayMicroseconds(10);
        }
    }

public:

  static inline bool setup(ESPNowSerialCommonLogFunc log_func_ptr = NULL, int i = WIFI_IF_AP) 
  {
    if(setup_done)
      return true;

    esp_err_t err;

    log_func = log_func_ptr;
/*
    if ((err = esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_MCS7_SGI)) != ESP_OK) 
    {
      ESPNOW_SERIAL_LOG_ERROR("esp_wifi_internal_set_fix_rate %i\n", (int)err);
      
      return false;
    }
*/

    rx_ring_buf = xRingbufferCreate(sizeof(PACKET) * 3, RINGBUF_TYPE_NOSPLIT);
    
    tx_ring_buf = xRingbufferCreate(255, RINGBUF_TYPE_BYTEBUF);
    serial_ring_buf = xRingbufferCreate(255, RINGBUF_TYPE_BYTEBUF);

    WiFi.setSleep(false);

    if ((err = esp_now_init()) != ESP_OK) 
    {
      ESPNOW_SERIAL_LOG_ERROR("esp_now_init %i\n", (int)err);
      
      return false;
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    memcpy(peer_info.peer_addr, peer_addr, sizeof(peer_addr));

    peer_info.channel = 0;  
    peer_info.encrypt = false;
      
    if ((err = esp_now_add_peer(&peer_info)) != ESP_OK)
    {
      ESPNOW_SERIAL_LOG_ERROR("esp_now_add_peer %i\n", (int)err);

      return false;
    }

    xTaskCreatePinnedToCore(loop, "espnow_serial", 4096, NULL, 0, &task, 0);

    ESPNOW_SERIAL_LOG_ERROR("ESPNowSerialCommon ch=%i\n", WiFi.channel());
    
    setup_done = true;

    return true;
  }

  static void write(const uint8_t* text, size_t len)
  {
    if(!setup_done)
      return;
    
    xRingbufferSend(tx_ring_buf, text, len, 0);
    
    xTaskNotify(task, NOTIFY_TX, eSetBits);
  }

  static int read()
  {
    if(!setup_done)
      return -1;

    size_t item_size;

    auto ptr = (uint8_t*)xRingbufferReceiveUpTo(serial_ring_buf, &item_size, 0, sizeof(uint8_t));

    int return_byte = -1;

    if(ptr)
    {
      return_byte = *ptr;

      vRingbufferReturnItem(serial_ring_buf, ptr);
    }

    return return_byte;
  }

  static int read(uint8_t* buffer, size_t len)
  {
    if(!setup_done)
      return -1;

    size_t item_size;

    auto ptr = (uint8_t*)xRingbufferReceiveUpTo(serial_ring_buf, &item_size, 0, len);

    int return_byte = -1;

    if(ptr)
    {
      memcpy(buffer, ptr, item_size);

      vRingbufferReturnItem(serial_ring_buf, ptr);
    }

    return return_byte;
  }

  static void try_find_device_channel()
  {
    esp_err_t err;
    int found_channel = 0;
    handshake_sem = xSemaphoreCreateBinary();

    PACKET packet = {{MAGIC_NUM, PACKET::HANDSHAKE, 0}};

    WiFi.macAddress(packet.handshake.mac);

    for(auto i = 1U; i <= 14U; i++)
    {
      ESPNOW_SERIAL_LOG("try_find_device_channel=%i\n", (int)i);

      esp_wifi_set_channel(i, WIFI_SECOND_CHAN_NONE);

      esp_now_send_packet(&packet);

      if(xSemaphoreTake(handshake_sem, pdMS_TO_TICKS(30)) == pdTRUE)
      {
        found_channel = i;
        break;
      }

      esp_now_send_packet(&packet);

      if(xSemaphoreTake(handshake_sem, pdMS_TO_TICKS(30)) == pdTRUE)
      {
        found_channel = i;
        break;
      }
    }

    vSemaphoreDelete(handshake_sem);

    /*
    if(found_channel > 0 && 0)
    {
      memcpy(peer_info.peer_addr, peer_addr, sizeof(peer_addr));

      peer_info.channel = found_channel;  
      peer_info.encrypt = false;
        
      if ((err = esp_now_add_peer(&peer_info)) != ESP_OK)
      {
        ESPNOW_SERIAL_LOG_ERROR("esp_now_add_peer %i\n", (int)err);
      }
    }
    */
    esp_wifi_set_channel(found_channel, WIFI_SECOND_CHAN_NONE);
    ESPNOW_SERIAL_LOG("Found channel=%i\n", (int)found_channel);
  }
};