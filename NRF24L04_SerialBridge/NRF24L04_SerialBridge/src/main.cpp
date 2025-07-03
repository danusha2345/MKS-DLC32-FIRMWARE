#include <Arduino.h>
#include "esp-now-uart.h"
#include <ArduinoOTA.h>

#define MKS_Serial Serial2

const char *ssid = "kv-41";
const char *password = "21302130";

//MAC:08:D1:F9:9A:B0:A8

void setup()
{
  Serial.begin(115200);

  uint8_t mks_mac[] = {0x38, 0xD1, 0x19, 0x9A, 0xC0, 0xE8};

  #if IS_MKS_BOARD
    MKS_Serial.setPins(26, 27);
    MKS_Serial.begin(115200, SERIAL_8N1);
    ESPNOW_SerialBridge::setup(&MKS_Serial, "MKS32 dongle", ssid, password, NULL);
    //MKS_Serial.onReceive(ESPNOW_SerialBridge::on_receive);
  #elif IS_CLIENT
    ESPNOW_SerialBridge::setup(&Serial, "MKS32 dongle", ssid, password, NULL, true);
  #elif IS_SERVER
    ESPNOW_SerialBridge::setup(&Serial, "MKS32 dongle SRC", ssid, password, NULL, false);
    //Serial.onEvent(ESPNOW_SerialBridge::on_receive);
  #endif

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else {  // U_SPIFFS
        type = "filesystem";
      }

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) {
        Serial.println("Auth Failed");
      } else if (error == OTA_BEGIN_ERROR) {
        Serial.println("Begin Failed");
      } else if (error == OTA_CONNECT_ERROR) {
        Serial.println("Connect Failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        Serial.println("Receive Failed");
      } else if (error == OTA_END_ERROR) {
        Serial.println("End Failed");
      }
    });

  ArduinoOTA.begin();
}

void loop()
{
  delay(30);

  ArduinoOTA.handle();
}
/*
#include <RF24.h>
#include <nRF24L01.h>

#define VERSION "0.1.1"

RF24 radio(9,10); // NRF24L01 used SPI pins + Pin 9 and 10 on the NANO

rf24_pa_dbm_e defaultPower = RF24_PA_LOW;
uint8_t defaultAddress[] = {0 ,0, 0, 0, 0};

void setup(void)
{
  Serial.begin(115200);
  
  Serial.println("INFO: RF Bridge " VERSION);

  radio.begin();
  radio.setPALevel(defaultPower);

  radio.openReadingPipe(1,defaultAddress);
  radio.openWritingPipe(defaultAddress);
}

void loop(void)
{
  uint8_t data[256];
  uint8_t pipe = 0;

  auto payload_size = radio.getPayloadSize();

  while(radio.available(&pipe)) 
  {
    radio.read(data, payload_size);
    Serial.write(&data[1U], data[0]);
  }

  auto avail = 0;

  while ((avail = Serial.available()) > 0)
  {
    auto len = Serial.read(&data[1U], std::min(payload_size, avail));

    data[0] = len;

    radio.stopListening();
    radio.write(data, len + 1);
    radio.startListening();
  }
}*/