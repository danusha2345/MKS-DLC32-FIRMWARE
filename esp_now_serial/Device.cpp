#include "Common.h"
#include "Serial.h"
#include "Device.h"
#include "Packet.h"

#include <esp_now.h>
#include <esp_wifi.h>

#include <Arduino.h>
#include <freertos\task.h>
#include <freertos\ringbuf.h>

/*
TaskHandle_t ESPNowSerialCommon::task = NULL;
RingbufHandle_t ESPNowSerialCommon::rx_ring_buf = NULL;
RingbufHandle_t ESPNowSerialCommon::tx_ring_buf = NULL;
RingbufHandle_t ESPNowSerialCommon::serial_ring_buf = NULL;
SemaphoreHandle_t ESPNowSerialCommon::handshake_sem = NULL;

bool ESPNowSerialCommon::setup_done = false;

uint64_t ESPNowSerialCommon::data_sent_at = 0;
uint16_t ESPNowSerialCommon::data_retry_count = 0;

ESPNowSerialCommon::packet_id_t ESPNowSerialCommon::next_data_packet_id = 0;
ESPNowSerialCommon::packet_id_t ESPNowSerialCommon::current_data_packet_id = 0;
*/

esp_now_peer_info_t peer_info = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, {}, 0, wifi_interface_t::WIFI_IF_STA, false};

size_t Packet::get_size()
{
    switch(header.type)
    {
    case DATA_RESPONCE:
        return sizeof(header) + sizeof(responce);

    case HANDSHAKE:
    case HANDSHAKE_REPONCE:
        return sizeof(header) + sizeof(handshake);

    case DATA:
        return sizeof(header) + sizeof(data.next_packet_id) + 
        sizeof(data.length) + data.length;

    default:
        return 0;
    }
}

bool Packet::is_valid(size_t size)
{
    if(size < sizeof(header))
        return false;

    if(header.magic_num != ESP_NOW_MAGIC_CONST)
        return false;

    size_t packet_size = get_sizeof(type)

    return true;
}

void Device::OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) 
{
    xTaskNotify(task, status == ESP_NOW_SEND_SUCCESS ? 0x1 : 0x2, eSetBits);
}

void Device::OnDataRecv(const uint8_t * mac_addr, const uint8_t *data, int len) 
{
    auto packet = (Packet*)data;

    if(!packet->is_valid(len))
    {
        ESPNOW_SERIAL_LOG("OnDataRecv invalid packet\n");
        return;
    }

    if(packet->header.to_device != ANY_DEVICE && packet->header.to_device != current_device)
        return;

    xRingbufferSend(rx_ring_buf, data, len, 0);
}

void Device::send_packet(Packet* packet)
{
    packet->header.from_device = current_device_id;
    
    ESPNOW_SERIAL_ASSERT(esp_now_send(peer_info.peer_addr, (uint8_t*)packet, packet->get_size())) == ESP_OK);

    uint32_t ulNotifiedValue = 0;

    auto res = xTaskNotifyWait(0x00, 0x1 | 0x2, &ulNotifiedValue, pdMS_TO_TICKS(5));

    if(ulNotifiedValue & 0x2)
    {
        ESPNOW_SERIAL_LOG("esp_now_send error\n");
    }
}

bool Device::setup(peer_id_t peer_id) 
{
    if(setup_done)
        return true;

    current_peer_id = peer_id;

    ESPNOW_SERIAL_ASSERT(esp_now_init() == ESP_OK);

    ESPNOW_SERIAL_ASSERT(rx_ring_buf = xRingbufferCreate(DEFAULT_TX_PACKET_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT));

    ESPNOW_SERIAL_ASSERT(rx_tx_queue = xQueueCreateSet(ESP_NOW_MAX_DEVICE_COUNT * 2));

    ESPNOW_SERIAL_ASSERT(xRingbufferAddToQueueSetRead(rx_ring_buf, rx_tx_queue) == pdTRUE);

    ESPNOW_SERIAL_ASSERT(esp_now_register_send_cb(OnDataSent) == ESP_OK);
    ESPNOW_SERIAL_ASSERT(esp_now_register_recv_cb(OnDataRecv) == ESP_OK);

    ESPNOW_SERIAL_ASSERT(esp_now_add_peer(&peer_info) == ESP_OK);

    ESPNOW_SERIAL_ASSERT(xTaskCreatePinnedToCore(loop, "espnow_serial", 4096, NULL, 0, &task, 0) == pdPASS);

    ESPNOW_SERIAL_LOG_ERROR("ESPNowSerialCommon ch=%i\n", WiFi.channel());
    
    setup_done = true;

    return true;
}

Peer* Device::find_peer(peer_id_t peer_id)
{
    for(auto i = 0; i < used_peers; i++)
    {
        if(peers[i]->peer_id == peer_id)
            return peers[i];
    }

    return nullptr;
}

bool Device::register_peer(Peer* peer)
{
    if(used_peers >= ESP_NOW_MAX_DEVICE_COUNT)
        return false;

    peers[used_peers++] = peer;

    return true;
}

void Device::loop(void* arg)
{
    esp_err_t err = ESP_OK;

    Packet packet;

    for(;;)
    {
        size_t size;
        uint32_t ulNotifiedValue;

        auto member = xQueueSelectFromSet(rx_tx_queue, portMAX_DELAY);

        if(xRingbufferCanRead(rx_ring_buf, member) == pdTRUE)
        {
            Packet* packet = (Packet*)xRingbufferReceive(rx_ring_buf, &size, 0);

            for(auto i = 0; i < used_peers; i++)
            {
                if(peers[i]->device_id == packet->header.to_device_id)
                    peers[i]->on_packet();
            }

            vRingbufferReturnItem(rx_ring_buf, packet);

            continue;
        }

        for(auto i = 0; i < used_peers; i++)
        {
            if(peers[i]->device_id == packet->header.to_device_id)
                peers[i]->on_tx_queue(member);
        }
    }
}