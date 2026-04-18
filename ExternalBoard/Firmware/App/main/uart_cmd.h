#pragma once
#include <stdint.h>

#pragma pack(push, 1)

#define EB_UART_HEADER_MAGIC 0xEB

#ifndef EB_UART_HEADER_UART_NUM 
    #define EB_UART_HEADER_UART_NUM UART_NUM_1
#endif

#ifndef EB_UART_CMD_MAX_DATA_SIZE
    #define EB_UART_CMD_MAX_DATA_SIZE 128
#endif

#ifndef EB_UART_CMD_LOG
    #define EB_UART_CMD_LOG(str, ...)
#endif

#ifndef UART_AVAIL
    #define UART_AVAIL() \
        (uart_get_buffered_data_len(UART_NUM_1, &size) == ESP_OK ? size : 0)
#endif

#ifndef UART_WRITE_BYTES
    #define UART_WRITE_BYTES(DATA, SIZE) \
        uart_write_bytes(UART_NUM_1, (uint8_t*)DATA, SIZE)
#endif

#ifndef UART_READ_BYTES
    #define UART_READ_BYTES(DATA, SIZE, TIMEOUT) \
        uart_read_bytes(UART_NUM_1, (uint8_t*)DATA, SIZE, TIMEOUT)
#endif

struct EB_UART_CMD
{
    enum EB_UART_CMD_TYPE : uint8_t
    {
        CMD_INPUT = 0x51,
        CMD_INPUT_CONNECTED = 0x52,
        CMD_INPUT_DISCONNECTED = 0x53,
        CMD_INPUT_ERROR = 0x54,
        
        CMD_SET_PIN = 0x61
    };

    enum STATE : uint8_t
    {
        STATE_WAIT,
        STATE_HEADER,
        STATE_DATA

    } state = STATE_WAIT;

    enum READ_RESULT : uint8_t
    {
        RESULT_CONTINUE,
        RESULT_WAITING,
        RESULT_ERROR,
        RESULT_DONE
    };

    using crc_t = uint16_t;
    using magic_t = uint8_t;

    struct HEADER
    {
        EB_UART_CMD_TYPE cmd;
        uint16_t length;
        crc_t crc;

    } header;

    uint8_t data[EB_UART_CMD_MAX_DATA_SIZE];

    size_t data_size;

    static crc_t calculate_crc(const void* ptr, uint16_t length);

    void send_cmd(EB_UART_CMD_TYPE cmd, const uint8_t* data = nullptr, uint16_t length = 0);
    
    READ_RESULT read_cmd();
};

READ_RESULT EB_UART_CMD::read_cmd()
{
    crc_t crc;
    magic_t magic;
    size_t size = 0;
    
    switch(state)
    {
        case STATE_WAIT:

            while(UART_READ_BYTES(&magic, sizeof(magic_t), 0))
            {
                if(magic == EB_UART_HEADER_MAGIC)
                {
                    state = STATE_HEADER;
                    return RESULT_CONTINUE;
                }
                else
                {
                    EB_UART_CMD_LOG("EB_UART_CMD error: invalid magic byte=%d", magic);
                    return RESULT_ERROR;
                }
            }

            return RESULT_WAITING;

        case STATE_HEADER:

            if(UART_AVAIL() < (sizeof(HEADER) + sizeof(crc_t)))
                return RESULT_WAITING;

            UART_READ_BYTES(&header, sizeof(header), 0);
            UART_READ_BYTES(&crc, sizeof(crc), 0);

            if(crc != calculate_crc(&header, sizeof(header)))
            {
                state = STATE_WAIT;
                EB_UART_CMD_LOG("EB_UART_CMD error in header crc=%d", crc);
                return RESULT_ERROR;
            }

            if(header.length == 0)
            {
                if(header.crc == 0xFFFF)
                {
                    data_size = 0;
                    state = STATE_WAIT;
                    return RESULT_DONE;
                }
                else
                {
                    EB_UART_CMD_LOG("EB_UART_CMD error in data=0 crc=%d", header.crc);

                    state = STATE_WAIT;
                    return RESULT_ERROR;
                }
            }
            else
            {
                if(header.length > sizeof(data))
                {
                    EB_UART_CMD_LOG("EB_UART_CMD error: data length %d exceeds max %d", 
                        header.length, EB_UART_CMD_MAX_DATA_SIZE);
                    
                    state = STATE_WAIT;
                    return RESULT_ERROR;
                }

                state = STATE_DATA;
                return RESULT_CONTINUE;
            }

        case STATE_DATA:
            
            if(UART_AVAIL() < header.length)
                return RESULT_WAITING;
            
            UART_READ_BYTES(data, MIN(header.length, sizeof(data)), 0);
            
            if(header.crc != calculate_crc(data, header.length))
            {
                EB_UART_CMD_LOG("EB_UART_CMD error in data crc=%d", header.crc);

                state = STATE_WAIT;
                return RESULT_ERROR;
            }
            
            state = STATE_WAIT;
            data_size = header.length;
            return RESULT_DONE;
    }

    return RESULT_ERROR;
}

EB_UART_CMD::crc_t EB_UART_CMD::calculate_crc(const void* ptr, uint16_t length)
{
    uint8_t* data = (uint8_t*)ptr;
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

void EB_UART_CMD::send_cmd(EB_UART_CMD_TYPE cmd, const uint8_t* data, uint16_t length)
{
    magic_t magic = EB_UART_HEADER_MAGIC;
    UART_WRITE_BYTES(&magic, sizeof(magic));

    header.cmd = cmd;
    header.length = length;
    
    header.crc = (data && length) ? 
        calculate_crc(data, length) :
            0xFFFF;

    UART_WRITE_BYTES(&header, sizeof(header));

    crc_t crc = calculate_crc(&header, sizeof(header));
    UART_WRITE_BYTES(&crc, sizeof(crc));

    if(data && length)
    {
        UART_WRITE_BYTES(data, length);
    }
}

#pragma pack(pop)