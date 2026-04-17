#pragma once

#pragma pack(push, 1)

enum EB_UART_CMD : uint8_t
{
    EB_UART_CMD_INPUT = 0x51,
    EB_UART_CMD_INPUT_CONNECTED = 0x52,
    EB_UART_CMD_INPUT_DISCONNECTED = 0x53,
    EB_UART_CMD_INPUT_ERROR = 0x54,
    
    EB_UART_CMD_SET_PIN = 0x61
};

#define EB_UART_HEADER_MAGIC 0xEB

struct EB_UART_HEADER
{
    uint8_t magic;
    enum EB_UART_CMD cmd;
    uint16_t length;
    uint16_t crc;
};

inline uint16_t eb_calculate_crc16(const uint8_t* data, uint16_t length) 
{
    uint16_t crc = 0xFFFF;

    for (uint16_t pos = 0; pos < length; pos++) 
    {
        crc ^= (uint16_t)data[pos];

        for (int i = 8; i != 0; i--)
        {
            if ((crc & 0x0001) != 0) 
            {
                crc >>= 1;
                crc ^= 0xA001;
            } 
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

#pragma pack(pop)