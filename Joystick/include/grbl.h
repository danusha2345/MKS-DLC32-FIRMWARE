#pragma once

#include <Arduino.h>
#include <GrblUtilities.h>
#include <GrblParser.h>
#include "wifi_helper.h"

#include <queue>

class TelnetGrblParser : public GrblParser
{
public:

    char temp_buffer[256] = {0};

    std::queue<char> buffer;

    TelnetGrblParser()
    {

    }

    void readLoop()
    {
        static int avail_bytes = 0;
        static char buffer[256];

        while(avail_bytes = WiFiHelper::telnet_client.available())
        {
            auto readed = WiFiHelper::telnet_client.readBytes(buffer, min<int>(avail_bytes, sizeof(buffer)));

            for(auto i = 0; i < readed; i++)
            {
                encode(buffer[i]);
            }
        }
    }

    void send(String str, uint32_t timeout = 0)
    {

    }

protected:
    [[nodiscard]] uint16_t available() override
    {
        int avail;
        
        if(avail = WiFiHelper::telnet_client.available())
        {
          auto readed = WiFiHelper::telnet_client.readBytes(temp_buffer, std::min<int>(avail, sizeof(temp_buffer)));

          for(auto i = 0; i < readed; i++)
            buffer.push(temp_buffer[i]);

          return readed + buffer.size();
        }

        return buffer.size();
    }
    
    [[nodiscard]] char read() override
    {
        if(buffer.empty())
          return -1;

        auto val = buffer.front();
        buffer.pop();
        return val;
    }

    void write(char c) override
    {
        WiFiHelper::telnet_client.write(c);
    }
};

extern TelnetGrblParser grblParser;