#include <Arduino.h>
#include <WiFi.h>

#include "../../esp_now_device/Device.h"
#include "../../esp_now_device/Device.cpp"

#ifdef IS_TEST_CLIENT
  #define ESP_NOW_DEVICE_ID 55
  #define ESP_NOW_DEVICE_PEER_ID 44
#endif

#ifdef IS_TEST_SERVER
  #define ESP_NOW_DEVICE_ID 44
  #define ESP_NOW_DEVICE_PEER_ID 55
#endif

using namespace EspNow;

#define ESP_NOW_SERIAL_CURRENT_PEER_ID 0x41
#define ESP_NOW_SERIAL_MKSDLC32_PEER_ID 0x42

Peer peer(ESP_NOW_SERIAL_MKSDLC32_PEER_ID);

void on_receive()
{
  int avail;
  uint8_t buffer[sizeof(Packet::data.data)];

  if((avail = Serial1.available()) > 0)
  {
    auto read_size = Serial1.readBytes(buffer, avail > sizeof(buffer) ? sizeof(buffer) : avail);

    peer.send(buffer, read_size, 3);
  }
}

void setup() 
{
  Serial1.setPins(23, 22);
  Serial1.begin(115200);

  Serial.begin(115200);

  //delay(1000);

  WiFi.mode(WIFI_AP_STA);

  //WiFi.begin("kv-41", "21302130");

  delay(2000);

  WiFi.setSleep(false);

  Serial.println("Start");

  Device::set_log_callback([](const char* data) -> void
  {
    Serial.print(data);
  });

  Device::setup(ESP_NOW_SERIAL_CURRENT_PEER_ID);

  delay(10);

  Device::add_peer(&peer);

  Serial1.onReceive(on_receive);

 // ESPNowSerial::setup([](const char* msg) -> void
  //{
    //  Serial.print(msg);
 // });

  #ifdef IS_TEST_CLIENT
    //ESPNowSerial::try_find_device_channel();
  #endif

  #ifdef IS_TEST_SERVER
    if(peer.try_connect(INT32_MAX, true))
      Serial.printf("Connected\n");
      else
      Serial.printf("Fail\n");
  #endif
}

int loop_index = 0;

void loop() 
{
  int avail = 0;
  int byte_val = 0;
  uint8_t buffer[sizeof(Packet::data.data)];

  static uint32_t last_send_ms = 0;

  size_t readed;

  if((readed = (peer.receive(buffer, sizeof(buffer), 1000))) > 0)
  {
    Serial1.write(buffer, readed);
  }

  
  if((millis() - last_send_ms) > 500)
  {
    /*
    #ifdef IS_TEST_SERVER
      char buff[32];
      sprintf(buff, "server %i\n", loop_index++);
      peer.send((uint8_t*)buff, strlen(buff), 0);
    #endif
    

    #ifdef IS_TEST_CLIENT
      char buff[32];
      sprintf(buff, "client %i\n", loop_index++);
      peer.send((uint8_t*)buff, strlen(buff), 0);
    #endif

    last_send_ms = millis();
    */
   
    Serial.printf("ping %i\n", (int)peer.get_avg_ping());
  }
}
