#include <Arduino.h>
#include <WiFi.h>

#include "../../esp_now_serial/ESPNowSerialDevice.h"
#include "../../esp_now_serial/ESPNowSerial.h"
#include "../../esp_now_serial/ESPNowSerial.cpp"

class ESPNowSerial : public ESPNowSerialCommon
{

};

void setup() 
{
  //Serial.setPins(23, 22);
  Serial.begin(115200);

  delay(1000);

  WiFi.mode(WIFI_AP_STA);

  Serial.println("Start");

  ESPNowSerial::setup([](const char* msg) -> void
  {
      Serial.print(msg);
  });

  #ifdef IS_TEST_CLIENT
    ESPNowSerial::try_find_device_channel();
  #endif

  #ifdef IS_TEST_SERVER
    ESPNowSerial::try_find_device_channel();
  #endif
}

int loop_index = 0;

void loop() 
{
  int avail = 0;
  int byte_val = 0;
  uint8_t buffer[256];

  static uint32_t last_send_ms = 0;

  while((byte_val = ESPNowSerial::read()) != -1)
  {
    Serial.write(byte_val);
  }

  while((avail = Serial.available()) > 0)
  {
    auto read_size = Serial.readBytes(buffer, avail > sizeof(buffer) ? sizeof(buffer) : avail);

    ESPNowSerial::write(buffer, read_size);
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
      ESPNowSerial::write((uint8_t*)buff, strlen(buff));
    #endif

    last_send_ms = millis();
  }

  delayMicroseconds(100);
}
