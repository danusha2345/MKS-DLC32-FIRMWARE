#pragma once

#include <stdint.h>
#include <Arduino.h>
#include <uButton.h>
#include <common.h>

class WiFiHelper
{
public:

    static inline const char* WIFI_SSID = "MKS_DLC";
    static inline const char* WIFI_PASSWORD = "12345678";

    static inline const char* TELNET_HOST = "192.168.4.1";
    static inline const uint16_t TELNET_PORT = 23;

    static inline WiFiClient telnet_client;
  
    static inline void connectWiFiIfNeeded()
    {
        if (WiFi.status() == WL_CONNECTED) 
            return;

        Serial.println();
        Serial.println("[WiFi] Connecting...");


        WiFi.mode(WIFI_STA);

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        uint32_t startAt = millis();

        bool led = true;

        while(WiFi.status() != WL_CONNECTED && (millis() - startAt) <= 5000)
        {
            digitalWrite(LED_PIN, led ? HIGH : LOW);
            led = !led;

            delay(100);
            Serial.print(".");
        }

        Serial.printf("Connected RSSI: %d IP: %s\n", WiFi.RSSI(), WiFi.localIP().toString().c_str());
    }

    static inline void connectTelnetIfNeeded() 
    {
        if (WiFi.status() != WL_CONNECTED) return;
        if (telnet_client.connected()) return;

        Serial.println();
        Serial.printf("[TCP] Connecting to %s:%u ...\n", TELNET_HOST, TELNET_PORT);

        telnet_client.stop();
        
        telnet_client.setTimeout(0);
        telnet_client.setConnectionTimeout(5000);

        if (telnet_client.connect(TELNET_HOST, TELNET_PORT)) 
        {
            Serial.println("[TCP] Connected");
            telnet_client.setNoDelay(true);
        } 
        else 
        {
            Serial.println("[TCP] Connect failed");
        }
    }

    static inline void wifiEvent(WiFiEvent_t event) 
    {
        Serial.printf("[WiFi] Event: %d\n", event);
    }

    static inline void wifi_thread(void* arg) 
    {
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.onEvent(wifiEvent);

        while(true) 
        {
            connectWiFiIfNeeded();
            connectTelnetIfNeeded();
            delay(300);
        }
    }

    static inline void setup()
    {
        xTaskCreate(wifi_thread, "wifi_thread", 4096, NULL, 1, NULL);
    }
};