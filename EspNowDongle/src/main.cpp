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

Peer peer(ESP_NOW_DEVICE_PEER_ID);

void setup() 
{
  //Serial.setPins(23, 22);
  Serial.begin(115200);

  delay(1000);

  WiFi.mode(WIFI_AP_STA);

  Serial.println("Start");

  Device::set_log_callback([](const char* data) -> void
  {
    Serial.print(data);
  });

  delay(2000);

  Device::setup(ESP_NOW_DEVICE_ID);

  delay(500);

  Device::add_peer(&peer);

 // ESPNowSerial::setup([](const char* msg) -> void
  //{
    //  Serial.print(msg);
 // });

  #ifdef IS_TEST_CLIENT
    //ESPNowSerial::try_find_device_channel();
  #endif

  #ifdef IS_TEST_SERVER
    peer.try_connect(INT32_MAX, true);
  #endif
}

int loop_index = 0;

void loop() 
{
  int avail = 0;
  int byte_val = 0;
  uint8_t buffer[256];

  static uint32_t last_send_ms = 0;

  size_t readed;

  if((readed = (peer.receive(buffer, sizeof(buffer), 0))) > 0)
  {
    Serial.write(buffer, readed);
  }

  while((avail = Serial.available()) > 0)
  {
    auto read_size = Serial.readBytes(buffer, avail > sizeof(buffer) ? sizeof(buffer) : avail);

    peer.send(buffer, read_size, 0);
  }

  if((millis() - last_send_ms) > 500)
  {
    
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

    Serial.printf("ping %i\n", (int)peer.get_avg_ping());
  }

  delay(300);
}
