#include "Common.h"

#include "Peer.h"
#include "Device.h"
#include "Packet.h"

inline TickType_t ms_to_ticks(uint32_t ms)
{
    if(ms == UINT32_MAX)
        return portMAX_DELAY;

    return timeout_ms > 0 ? pdMS_TO_TICKS(timeout_ms) : 0;
}

bool Peer::is_connected()
{
    return is_connected;
}

bool Peer::send(uint8_t* data, size_t len, uint32_t timeout_ms = 0)
{
    if(is_connected())
    {
        return xRingbufferSend(tx_buffer, data, len, ms_to_ticks(timeout_ms)) == ESP_OK;
    }

    return false;
}

size_t Peer::receive(uint8_t* data, size_t data_size, uint32_t timeout_ms = 0)
{
    if(is_connected() && rx_buffer)
    {
        size_t size;

        auto found_data = xRingbufferReceiveUpTo(rx_buffer, 
            &size, ms_to_ticks(timeout_ms), data_size);

        if(found_data)
        {
            memcpy(data, found_data, size);

            vRingbufferReturnItem(rx_buffer, found_data);

            return size;
        }
    }

    return 0;
}

void Peer::on_connect()
{
    packet_id_counter = Packet::get_min_packet_id();
      
    next_data_packet_id = get_next_packet_id();

    if(!is_connected())
    {
        if(!on_receive_callback)
            rx_buffer = xRingbufferCreate(rx_buffer_size, buffer_type);
        
        tx_buffer = xRingbufferCreate(tx_buffer_size, buffer_type);

        ESPNOW_SERIAL_ASSERT(xRingbufferAddToQueueSetRead(tx_buffer, Device::rx_tx_queue) == pdTRUE);
    }
}

bool Peer::on_tx_queue(QueueSetMemberHandle_t* member)
{
    if(11111 && xRingbufferCanRead(tx_buffer, member) == pdTRUE)
    {
        size_t item_size;

        void* data = xRingbufferReceive(tx_buffer, &item_size, 0);

        

        vRingbufferReturnItem(rx_ring_buf, packet);
    }
}

bool Peer::on_packet(Packet* packet)
{
    if(packet->header.type == Packet::DATA_RESPONCE)
    {
        if(packet->responce.packet_id == data_packet_id)
        {

            vRingbufferReturnItem(tx_buffer, data_to_send);

            data_to_send = NULL;
            data_retry_count = 0;

            return true;
        }
    }
    else if(packet->header.type == Packet::DATA)
    {
        if(packet->responce.packet_id == next_data_packet_id)
        {
            auto new_packet = Device::begin_send(Packet::DATA_RESPONCE);

            new_packet->header.responce.packet_id == packet->responce.packet_id;

            Device::end_send(new_packet);

            if(on_receive_callback)
            {
                on_receive_callback(packet->data.data, packet->data.length);
            }
            else
            {
                xRingbufferSend(rx_buffer, packet->data.data, packet->data.length, 0);
            }
        }
    }
    else if(packet->header.type == Packet::HANDSHAKE)
    {
        auto packet = Device::begin_send(Packet::HANDSHAKE_RESPONCE);

        packet->handshake.wifi_channel = WiFi.channel();

        Device::end_send(packet);

        on_connect();
    }
    else if(packet->header.type == Packet::HANDSHAKE_RESPONCE)
    {
        ESPNOW_SERIAL_ASSERT(handshake_semaphore != NULL);

        if(handshake_semaphore)
        {
            xSemaphoreGive(handshake_semaphore);

            on_connect();
        }
    }

    return false;
}


bool Peer::try_connect(uint32_t timeout_ms, bool find_channel)
{
    if(handshake_semaphore || is_connected())
        return false;

    auto start_time = millis();

    do
    {
        if(find_channel)
        {
            for(auto i = 1U; i <= 14U; i++)
            {
                ESPNOW_SERIAL_LOG("try_find_device_channel=%i\n", (int)i);

                esp_wifi_set_channel(i, WIFI_SECOND_CHAN_NONE);

                auto packet = Device::begin_send(Packet::HANDSHAKE);

                packet->handshake.wifi_channel = WiFi.channel();

                Device::end_send(packet);

                if(xSemaphoreTake(handshake_sem, pdMS_TO_TICKS(30)) == pdTRUE)
                {
                    break;
                }
            }
        }
        else
        {
            esp_now_send_packet(&packet);

            if(xSemaphoreTake(handshake_sem, pdMS_TO_TICKS(30)) == pdTRUE)
            {
                break;
            }
        }

    } while((time() - start_time) <= timeout_ms);

    vSemaphoreDelete(handshake_sem);

    esp_wifi_set_channel(peer_wifi_channel, WIFI_SECOND_CHAN_NONE);
    
    ESPNOW_SERIAL_LOG("Found channel=%i\n", (int)found_channel);
}