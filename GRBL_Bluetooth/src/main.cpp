
#include <stdio.h>
#include <stddef.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "BluetoothSerial.h"
#include "ESPTelnet.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#define MKS_Serial Serial2

BluetoothSerial SerialBT;
ESPTelnet Telnet;

void onBtData(const uint8_t *buffer, size_t size)
{
  MKS_Serial.write(buffer, size);
}

char buffer[128];

void onMksData()
{
  int avail_bytes;

  while((avail_bytes = MKS_Serial.available()) > 0)
  {
    auto readed = MKS_Serial.readBytes(buffer, min<int>(avail_bytes, sizeof(buffer)));

    Telnet.write((uint8_t*)buffer, readed);
    SerialBT.write((uint8_t*)buffer, readed);
    Serial.write((uint8_t*)buffer, readed);
  }
}

void onSerialData()
{
   int avail_bytes;

    while((avail_bytes = Serial.available()) > 0)
    {
      auto readed = Serial.readBytes(buffer, min<int>(avail_bytes, sizeof(buffer)));

      MKS_Serial.write((uint8_t*)buffer, readed);
    }
}

void onTelnetData(String data)
{
    MKS_Serial.write((uint8_t*)data.c_str(), data.length());
}

volatile int bt_connection_event = 0;

void Bt_Status (esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {

  if (event == ESP_SPP_SRV_OPEN_EVT) 
  {
    bt_connection_event = 30;
  }
}

const char *ssid = "kv-41";
const char *password = "21302130";

void setup() 
{

  Serial.begin(115200);

  SerialBT.begin("MKS32"); //Bluetooth device name

  MKS_Serial.setPins(26, 27);
  MKS_Serial.begin(115200, SERIAL_8N1);

  SerialBT.onData(onBtData);
  Serial.onReceive(onSerialData);
  MKS_Serial.onReceive(onMksData);
  Telnet.onInputReceived(onTelnetData);

  SerialBT.register_callback(Bt_Status);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  for(auto i = 0; i < 20; i++)
  {
    if(WiFi.waitForConnectResult() == WL_CONNECTED)
      break;

    delay(100);
  }

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

  Telnet.begin();   
  
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}


void loop() 
{
  Telnet.loop();

  delay(3);

  ArduinoOTA.handle();

  delay(3);

  if(bt_connection_event > 0)
  {
    bt_connection_event--;

    if(bt_connection_event == 0)
      SerialBT.println(WiFi.localIP());
  }
}