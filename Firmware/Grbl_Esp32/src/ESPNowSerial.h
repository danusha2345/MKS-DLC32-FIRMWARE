#include <esp_now.h>
#include <esp_wifi.h>

#include <WiFi.h>
#include <FreeRTOS.h>
#include "../../esp_now_device/Device.h"

#define ESP_NOW_SERIAL_CURRENT_PEER_ID 0x42
#define ESP_NOW_SERIAL_DONGLE_PEER_ID 0x41

class ESPNowSerial
{
    static bool setup_done;
    static EspNow::Peer peer;

public:

    static void set_log_callback(EspNow::EspNowDeviceLog clb)
    {
        EspNow::Device::set_log_callback(clb);
    };

    static void setup()
    {
        if(setup_done)
            return;

        setup_done = true;

        WiFi.setSleep(false);

        EspNow::Device::setup(ESP_NOW_SERIAL_CURRENT_PEER_ID);

        EspNow::Device::add_peer(&peer);
    }

    static int read()
    {
        uint8_t byte_val;

        if(peer.receive(&byte_val, 1, 0))
            return byte_val;

        return -1;
    }

    static void write(uint8_t* data, size_t len)
    {
        peer.send(data, len, 0);
    }
};