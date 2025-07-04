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

void setup() 
{
  //Serial.setPins(23, 22);
  Serial.begin(115200);

  delay(1000);

  WiFi.mode(WIFI_AP_STA);

  Serial.println("Start");

  EspNowDevice::set_log_callback([](const char* data) -> void
  {
    Serial.print(data);
  });

  EspNowDevice::setup(ESP_NOW_DEVICE_ID);

  EspNowDevice::add_peer(ESP_NOW_DEVICE_PEER_ID);

 // ESPNowSerial::setup([](const char* msg) -> void
  //{
    //  Serial.print(msg);
 // });

  #ifdef IS_TEST_CLIENT
    //ESPNowSerial::try_find_device_channel();
  #endif

  #ifdef IS_TEST_SERVER
    EspNowDevice::try_connect(ESP_NOW_DEVICE_PEER_ID, INT32_MAX, true);
  #endif
}

int loop_index = 0;

void loop() 
{
  int avail = 0;
  int byte_val = 0;
  uint8_t buffer[256];

  static uint32_t last_send_ms = 0;

  //while((byte_val = ESPNowSerial::read()) != -1)
  {
    //Serial.write(byte_val);
  }

  while((avail = Serial.available()) > 0)
  {
    auto read_size = Serial.readBytes(buffer, avail > sizeof(buffer) ? sizeof(buffer) : avail);

    //ESPNowSerial::write(buffer, read_size);
  }

  if((millis() - last_send_ms) > 100 && 0)
  {
    /*
    #ifdef IS_TEST_SERVER
      char buff[32];
      sprintf(buff, "server %i\n", loop_index++);
      ESPNowSerial::write((uint8_t*)buff, strlen(buff));
    #endif
    */

    #ifdef IS_TEST_CLIENT
      char buff[32];
      sprintf(buff, "client %i\n", loop_index++);
      //ESPNowSerial::write((uint8_t*)buff, strlen(buff));
    #endif

    last_send_ms = millis();
  }

  delayMicroseconds(100);
}
