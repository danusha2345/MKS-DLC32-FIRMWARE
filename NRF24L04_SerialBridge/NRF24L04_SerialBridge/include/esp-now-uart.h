#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_private/wifi.h>

#include <WiFi.h>

/*
// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
    char a[32];
    int b;
    float c;
    bool d;
} struct_message;

// Create a struct_message called myData
struct_message myData;

// callback function that will be executed when data is received

 
void setup() 
{
  // Initialize Serial Monitor
  Serial.begin(115200);
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);
}
 
void loop() 
{

}
*/

/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp-now-esp32-arduino-ide/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>
#include <freertos/message_buffer.h>
#include "ESPTelnet.h"

#define MAX_PACKET_LENGTH 250
#define MESSAGE_BUFFER_SIZE (MAX_PACKET_LENGTH * 2)

class ESPNOW_SerialBridge
{
  static inline TaskHandle_t tx_task;
  static inline TaskHandle_t main_task;
  static inline TaskHandle_t common_task;

  static inline MessageBufferHandle_t rx_queue;
  static inline MessageBufferHandle_t tx_queue;

  static inline QueueHandle_t send_now_sem;

  static inline Stream* stream;

  static inline IPAddress local_ip;
  static inline uint8_t local_mac_addr[6];
  
  static inline StreamBufferHandle_t serial_stream;

  #pragma pack(push, 1)

  struct ESP_MSG
  {
    enum MSG_TYPE : uint8_t
    {
      PING,
      PING_RESPONCE,
      SERIAL_DATA,
      RESPONCE,

      HANDSHAKE,
      HANDSHAKE_RESPONCE,
    };

    struct
    {
      MSG_TYPE type;
      uint32_t packet_id;
      uint64_t time_us;

    } header;

    union
    {
      struct
      {
        uint64_t time_us;

      } responce;

      struct
      {
        uint64_t time;

      } ping;

      struct
      {
        uint64_t time;

      } ping_responce;

      struct
      {
        uint8_t length;

        uint8_t buffer[MAX_PACKET_LENGTH - sizeof(header) - sizeof(length)];
        
      } serial_data;

      struct
      {
        uint32_t ip;
        uint64_t mac[6];
        
      } handshake;

    };
  
    size_t get_msg_size()
    {
      switch (header.type)
      {
      case RESPONCE:
        return sizeof(header) + sizeof(responce);

      case PING:
        return sizeof(header) + sizeof(ping);

      case PING_RESPONCE:
        return sizeof(header) + sizeof(ping_responce);

      case SERIAL_DATA:
        return sizeof(header) + sizeof(serial_data.length) + serial_data.length;

      case HANDSHAKE:
      case HANDSHAKE_RESPONCE:
        return sizeof(header) + sizeof(handshake);
      }
    }
  };

  #pragma pack(pop) 

  static inline uint32_t packet_id = 0;

  static inline uint8_t broadcast_address[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  static inline esp_now_peer_info_t peer_info;

  static void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
  {
    //xSemaphoreGive(send_now_sem);
    //xTaskNotify(tx_task, status == ESP_NOW_SEND_SUCCESS ? 0x1 : 0x2, eSetBits);
  }

  static void OnDataRecv(const uint8_t * mac, const uint8_t *incoming_data, int len) 
  {
    auto msg = (ESP_MSG*)incoming_data;

    if(1 || msg->header.type == ESP_MSG::SERIAL_DATA)
    {
      stream->write(incoming_data, len);
    }
    else
    {
      xMessageBufferSend(rx_queue, incoming_data, len, portMAX_DELAY);
    }
  }

  static inline bool is_new_line(uint8_t data)
  {
      if(data == '\r' || data == '\n')
        return true;

      if (data >= 0x80) 
      {
          return true;
      }

      return data == 24U || data == 63 || data == 126 || data == 33U;
  }

  static inline SemaphoreHandle_t serial_block_sem;

  static void common_task_loop(void* param)
  {
      ESP_MSG msg = {};

      uint8_t buf[250];
      uint64_t buf_pos = 0;
      
      size_t avail_len = 0;

      for(;;)
      {
        uint8_t byte = 0;

        while((avail_len = stream->available()) > 0)
        {
          auto readed = stream->readBytes(buf, avail_len);

          esp_now_send(broadcast_address, (uint8_t*)buf, readed);

          //if(val != -1)
          {
           // uint8_t byte_val = (uint8_t)val;

            //xStreamBufferSend(serial_stream, &byte_val, sizeof(byte_val), portMAX_DELAY);
          }
        }

        delayMicroseconds(100);
        //auto recived = xStreamBufferReceive(serial_stream, buf, sizeof(buf), portMAX_DELAY);

       // if(recived > 0)
        {
          //esp_now_send(broadcast_address, (uint8_t*)buf, recived);

          //buf[buf_pos++] = byte;

          //if(buf_pos >= sizeof(buf) || is_new_line(byte))
          {
           // esp_now_send(broadcast_address, (uint8_t*)buf, buf_pos);

            //msg.header.type = ESP_MSG::SERIAL_DATA;
            //msg.serial_data.length = buf_pos;
            //memcpy(msg.serial_data.buffer, buf, buf_pos);

            //buf_pos = 0;
           // send_msg(msg);
            
            //if(xSemaphoreTake(serial_block_sem, pdMS_TO_TICKS(20)) != pdTRUE)
            {
              //Serial.printf("x\n");
            }
          }

          //xSemaphoreTake(serial_block_sem, portMAX_DELAY);
      }
    }
  }

  static void tx_task_loop(void* param)
  {
    uint32_t ulNotifiedValue;

    ESP_MSG msg = {};

    ESP_MSG responce_msg = {};

    for(;;)
    {
      auto size = xMessageBufferReceive(rx_queue, &msg, sizeof(msg), 0);

      if(size > 0)
      {
        switch(msg.header.type)
        {
          case ESP_MSG::RESPONCE:
            xSemaphoreGive(serial_block_sem);
            //Serial.printf("RESP %i\n", (int)(micros() - msg.responce.time_us));
            break;

          case ESP_MSG::SERIAL_DATA:

            stream->write(msg.serial_data.buffer, msg.serial_data.length);

            responce_msg = {{ESP_MSG::RESPONCE}};
            responce_msg.responce.time_us = msg.header.time_us;

            //send_msg(responce_msg);

            break;
          
          case ESP_MSG::HANDSHAKE:

            msg = {{ESP_MSG::HANDSHAKE_RESPONCE}};

            memcpy(msg.handshake.mac, local_mac_addr, sizeof(local_mac_addr));
            msg.handshake.ip = (uint32_t)local_ip;

            send_msg(msg);

            break;

          case ESP_MSG::HANDSHAKE_RESPONCE:

            Serial.printf("Remote IP:%s\n", IPAddress(msg.handshake.ip).toString().c_str());

            break;


          case ESP_MSG::PING:
            msg = {{ESP_MSG::PING_RESPONCE}};
            msg.ping_responce.time = msg.ping.time;
            send_msg(msg);
            break;
          
          case ESP_MSG::PING_RESPONCE:
            Serial.printf("PING %i\n", (int)(micros() - msg.ping_responce.time));
            break;
        }
      }

      delayMicroseconds(100);
    }
    /*
    for(;;)
    {
      uint8_t buffer[MAX_PACKET_LENGTH];

      auto size = xMessageBufferReceive(tx_queue, buffer, MAX_PACKET_LENGTH, portMAX_DELAY);

      if(size > 0)
      {
        esp_err_t result = esp_now_send(broadcast_address, buffer, size);
      
        if (result == ESP_OK) 
        {
          xTaskNotifyWaitIndexed(0, 0x0, ULONG_MAX, &ulNotifiedValue, portMAX_DELAY);

          if(ulNotifiedValue & 0x2)
          {
            Serial.printf("Packet loss\n");
          }
        }
      }

    }*/
  }

  static void send_msg(ESP_MSG &msg)
  {
    msg.header.time_us = micros();
    msg.header.packet_id = ++packet_id;

    //xSemaphoreTake(send_now_sem, portMAX_DELAY);

    esp_now_send(broadcast_address, (uint8_t*)&msg, msg.get_msg_size());


    //xSemaphoreTake(send_now_sem, portMAX_DELAY);

    //xSemaphoreTake(sem, portMAX_DELAY);
    //xMessageBufferSend(tx_queue, &msg, msg.get_msg_size(), pdMS_TO_TICKS(5000));
  }

  int loop_counter = 0;


  static void main_task_loop(void*) 
  {
    unsigned long last_ping_time = 0;

    char buf[256];

    uint16_t buf_pos = 0;

    for(;;)
    {
      size_t avail_len = 0;

      while((avail_len = stream->available()) > 0)
      {
        auto val = stream->read();

        if(val != -1)
        {
          uint8_t byte_val = (uint8_t)val;

          
          buf[buf_pos++] = val;

          //if(byte_val == 
          
          //xStreamBufferSend(serial_stream, &byte_val, sizeof(byte_val), portMAX_DELAY);
        }
      }

      /*
      if((avail_len = stream->available()) > 0)
      {
        xStreamBufferSend(serial_stream, )

        ESP_MSG msg = {{ESP_MSG::SERIAL_DATA}};

        int byte_val = 0;
        uint8_t buffer_pos = 0;

        /*while((byte_val = stream->read()) != -1)
        {
          msg.serial_data.buffer[buffer_pos++] = byte_val;
          
          if(byte_val == 0 || byte_val == '\r' || byte_val == '\n' || buffer_pos >= sizeof(msg.serial_data.buffer))
          {
            //break;
          }
        }

        auto len = stream->readBytes(msg.serial_data.buffer, 
            avail_len > sizeof(msg.serial_data.buffer) ? 
              sizeof(msg.serial_data.buffer) : avail_len);

        msg.serial_data.length = len;

        send_msg(msg);

        xSemaphoreTake(sem2, portMAX_DELAY);
      }
      */

      #if 0
        auto now = millis();

        if(0 && (now - last_ping_time) > 1000)
        {
            msg = {{ESP_MSG::PING}};
            msg.ping.time = micros();

            send_msg(msg);
            last_ping_time = now;
        }
      #endif

      delayMicroseconds(5);
    }
  }

public:


  static inline void on_receive()
  {
      size_t avail_len = 0;

      static uint8_t buffer[256];

      static uint16_t buffer_pos = 0;

      while((avail_len = stream->available()) > 0)
      {
        auto val = stream->read();

        if(val != -1)
        {
          uint8_t byte_val = (uint8_t)val;

          xStreamBufferSend(serial_stream, &byte_val, sizeof(byte_val), portMAX_DELAY);
        }
      }
  }

  static void mks()
  {
     WiFiServer server(10000);
    WiFiClient client;

    server.begin();
  
    int length = 0;
    int avail_len = 0;

    char buf[512];

    for(;;)
    {
      if (!client)
        client = server.available();

        if(client)
        {
           if ((length = client.available()) > 0) 
           {
              client.readBytes(buf, length);
              stream->write(buf, length);
           }

          while((avail_len = stream->available()) > 0)
          {
              stream->readBytes(buf, avail_len);
              client.write(buf, avail_len);
          }
        }

        delay(1);
    }
  }

  static void dongle()
  {
    WiFiClient client;

    int ii = client.connect("192.168.0.1", 23, 5000);

    Serial.printf("Con %i\n", ii);
  
    int length = 0;
    int avail_len = 0;

    char buf[512];

    for(;;)
    {
        if(!client.connected())
        {
          delay(100);
          continue;
        }

        {
           if ((length = client.available()) > 0) 
           {
              client.readBytes(buf, length);
              stream->write(buf, length);
           }
        }

        while((avail_len = stream->available()) > 0)
        {
            stream->readBytes(buf, avail_len);
            client.write(buf, avail_len);
        }

        delay(1);
    }
  }

  static inline bool setup(Stream* main_stream, const char* host_name, const char* ssid, const char* pass, uint8_t* mac_addr, bool set_mac = false) 
  {
    serial_stream = xStreamBufferCreate(512, 1U);

    serial_block_sem = xSemaphoreCreateBinary();
    send_now_sem = xSemaphoreCreateBinary();

    if(mac_addr)
    {
      if(set_mac)
      {
        esp_base_mac_addr_set(mac_addr);
      }
      else
      {
        memcpy(broadcast_address, mac_addr, sizeof(broadcast_address));
      }
    }

    stream = main_stream;
    // Init Serial Monitor

    rx_queue = xMessageBufferCreate(MESSAGE_BUFFER_SIZE);
    tx_queue = xMessageBufferCreate(MESSAGE_BUFFER_SIZE);

    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_AP_STA);
  
    esp_wifi_internal_set_fix_rate(wifi_interface_t::WIFI_IF_STA, true, WIFI_PHY_RATE_MCS7_SGI);

    if(host_name)
      WiFi.setHostname(host_name);

    WiFi.setSleep(false);

    WiFi.begin("MKSDLC32", "MKSDLC32");

    auto start_time = millis();

    do
    {
      if(WiFi.status() == WL_CONNECTED)
      {
        local_ip = WiFi.localIP();
        WiFi.macAddress(mac_addr);

        Serial.printf("WiFi %s %s connected, IP:%s MAC:%s\n", ssid, pass, 
          local_ip.toString().c_str(), WiFi.macAddress().c_str());
          
        break;
      }

    } while(millis() - start_time <= 15000);

    if(WiFi.status() != WL_CONNECTED)
      Serial.printf("WiFi %s %s connect FAIL\n", ssid, pass);

    #if IS_MKS_BOARD
      mks();
    #else
      dongle();
    #endif
   
    return 1;

    WiFi.macAddress(local_mac_addr);

    if (esp_now_init() != ESP_OK) 
    {
      return false;
    }

    esp_now_register_send_cb(OnDataSent);
    esp_now_register_recv_cb(OnDataRecv);

    memcpy(peer_info.peer_addr, broadcast_address, sizeof(broadcast_address));

    peer_info.channel = 0;  
    peer_info.encrypt = false;
      
    if (esp_now_add_peer(&peer_info) != ESP_OK)
    {
      return false;
    }

    xTaskCreatePinnedToCore(tx_task_loop, "espnow_tx_task", 4096, NULL, 0, &tx_task, 0);
    //xTaskCreatePinnedToCore(main_task_loop, "espnow_main_task", 4096, NULL, 0, &main_task, 1);
    xTaskCreatePinnedToCore(common_task_loop, "espnow_common", 4096, NULL, 0, &common_task, 0);

    /*
#if IS_SERVER
    ESP_MSG msg = {{ESP_MSG::HANDSHAKE}};
    esp_wifi_get_mac((wifi_interface_t)ESP_IF_WIFI_STA, msg.handshake.mac);

    send_msg(msg);
#endif
    */

    return true;
  }
};