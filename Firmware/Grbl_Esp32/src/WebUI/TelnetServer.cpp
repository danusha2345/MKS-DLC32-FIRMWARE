/*
  TelnetServer.cpp -  telnet server functions class

  Copyright (c) 2014 Luc Lebosse. All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "../Grbl.h"

#if defined(ENABLE_WIFI) && defined(ENABLE_TELNET)

#    include "WifiServices.h"

#    include "TelnetServer.h"
#    include "WifiConfig.h"
#    include <WiFi.h>

namespace WebUI {
    Telnet_Server telnet_server[TELNET_CLIENTS_TOTAL];
    uint16_t      Telnet_Server::_port         = 0;
    AsyncServer*   Telnet_Server::_telnetserver = NULL;

    #ifdef ENABLE_TELNET_OTHER_TASK
        TaskHandle_t _telnet_task;
    #endif

    #define CLIENT_TELNET_INDEX(client) (client - CLIENT_TELNET_MIN)
    #define CLIENT_TELNET_VAL(i) (i + CLIENT_TELNET_MIN)

    void telnet_handle_client(void *arg, AsyncClient *client) 
    {
        for(auto i = 0; i < TELNET_CLIENTS_TOTAL; i++)
        {
            if (!telnet_server[i].is_connected()) 
            {
                telnet_server[i].setup_client(client);

                String s = "[MSG:TELNET Connected i:" + String(i) + "]\r\n";
                grbl_send(CLIENT_ALL, (char*)s.c_str());
                break;
            }
        }
    }

    void Telnet_Server::setup_client(AsyncClient* client)
    {
        _telnetClientIP = IPAddress(0, 0, 0, 0);

        _telnetClient = client;
        

        client->onData([](void *arg, AsyncClient *client, void *data, size_t len) 
        {
            auto ptr = static_cast<Telnet_Server*>(arg);
            ptr->push((uint8_t*)data, len);

        }, this);
    }


    void Telnet_Server::begin_all()
    {
        if (telnet_enable->get() == 0) {
            return;
        }

        _port = telnet_port->get();

        //create instance
        _telnetserver = new AsyncServer(_port);
        _telnetserver->setNoDelay(true);
        
        String s = "[MSG:TELNET Started " + String(_port) + "]\r\n";
        grbl_send(CLIENT_ALL, (char*)s.c_str());
        //start telnet server
        _telnetserver->onClient(&telnet_handle_client, nullptr);
        _telnetserver->begin();

        for(auto i = 0; i < TELNET_CLIENTS_TOTAL; i++)
        {
            telnet_server[i]._client_index = i;
            telnet_server[i].begin();
        }

        #ifdef ENABLE_TELNET_OTHER_TASK
            xTaskCreatePinnedToCore([](void* arg) 
            {
                while(true)
                {
                    Telnet_Server::_handle_all_real();
                    delayMicroseconds(100);
                }
            },    // task
            "telnet_task",  // name for task
            4096,               // size of task stack
            NULL,               // parameters
            2,                  // priority
            &_telnet_task,
            SUPPORT_TASK_CORE);  // must run the task on same core
        #endif
    }

    void Telnet_Server::end_all()
    {
        #ifdef ENABLE_TELNET_OTHER_TASK
            if(_telnet_task)
            {
                vTaskDelete(_telnet_task);
                _telnet_task = NULL;
            }
        #endif

        if (_telnetserver) 
        {
            _telnetserver->end();
            delete _telnetserver;
            _telnetserver = NULL;
        }

        for(auto i = 0; i < TELNET_CLIENTS_TOTAL; i++)
            telnet_server[i].end();
    }

    void Telnet_Server::handle_all()
    {
        if(_telnetserver == NULL)
            return;

        #ifndef ENABLE_TELNET_OTHER_TASK
            _handle_clients();
        #endif
    }

    void Telnet_Server::_handle_clients()
    {
        for(auto i = 0; i < TELNET_CLIENTS_TOTAL; i++)
            telnet_server[i].handle();
    }

    void Telnet_Server::write(uint8_t client, const uint8_t* buffer, size_t size)
    {
        if(client == CLIENT_ALL)
        {
            for(auto i = 0; i < TELNET_CLIENTS_TOTAL; i++)
                telnet_server[i].write(buffer, size);
        }
        else
        {
            uint8_t index = CLIENT_TELNET_INDEX(client);

            if(index >= 0 && index < TELNET_CLIENTS_TOTAL)
                telnet_server[index].write(buffer, size);
        }
    }

    
    bool Telnet_Server::read(char* code, uint8_t* client)
    {
        for(auto i = 0; i < TELNET_CLIENTS_TOTAL; i++)
        {
            if(telnet_server[i].available())
            {
                *code = telnet_server[i].read();
                *client = CLIENT_TELNET_VAL(i);
                return true;
            }
        }

        return false;
    }

    int Telnet_Server::get_rx_buffer_available(uint8_t client)
    {
        for(auto i = 0; i < TELNET_CLIENTS_TOTAL; i++)
        {
            if(CLIENT_TELNET_VAL(i) == client)
            {
                return WebUI::telnet_server[i].get_rx_buffer_available();
            }
        }

        return 0;
    }

    Telnet_Server::Telnet_Server() 
    {
    }

    bool Telnet_Server::begin() 
    {
        end();

        if(_ring_buffer == NULL)
            _ring_buffer = xQueueCreate(TELNETRXBUFFERSIZE, sizeof(char));

        return true;
    }

    void Telnet_Server::end() 
    {
        _setupdone    = false;
        _telnetClient = NULL;
    }

    size_t Telnet_Server::write(const uint8_t* buffer, size_t size) {
        size_t wsize = 0;
        if (!_setupdone) {
            log_d("[TELNET out blocked]");
            return 0;
        }
        
        //log_d("[TELNET out]");
        //push UART data to all connected telnet clients
        if (is_connected())
        {
            //log_d("[TELNET out connected]");
            
            wsize = _telnetClient->write((char*)buffer, size);
            COMMANDS::wait(0);
        }

        return wsize;
    }

    void Telnet_Server::handle()
    {
        COMMANDS::wait(0);
        //check if can read
        if (!_setupdone) {
            return;
        }

        //check clients for data
        //uint8_t c;

        if (is_connected()) 
        {
#    ifdef ENABLE_TELNET_WELCOME_MSG
            if (_telnetClientIP != _telnetClient->remoteIP()) 
            {
                report_init_message(CLIENT_TELNET_VAL(_client_index));
                _telnetClientIP = _telnetClient->remoteIP();
            }
#    endif
        } 

        COMMANDS::wait(0);
    }

    int Telnet_Server::peek(void) 
    {
        uint8_t byte;

        if(_ring_buffer && xQueuePeek(_ring_buffer, &byte, 0) == pdTRUE)
            return byte;

        return -1;
    }

    int Telnet_Server::available() { return _ring_buffer ? uxQueueMessagesWaiting(_ring_buffer) : 0; }

    int Telnet_Server::get_rx_buffer_available() { return _ring_buffer ? uxQueueSpacesAvailable(_ring_buffer) : 0; }

    bool Telnet_Server::push(uint8_t data) 
    {
        return _ring_buffer && xQueueSend(_ring_buffer, &data, 0) == pdTRUE;
    }

    bool Telnet_Server::push(const uint8_t* data, int data_size) 
    {
        if(_ring_buffer)
        {
            for(auto i = 0; i < data_size; i++)
                xQueueSend(_ring_buffer, &data[i], 0);
        }

        return true;
    }

    int Telnet_Server::read(void) {

        uint8_t byte = 0;

        if(_ring_buffer && xQueueReceive(_ring_buffer, &byte, 0))
            return byte;

        return -1;
    }

    Telnet_Server::~Telnet_Server() { end(); }
}
#endif  // Enable TELNET && ENABLE_WIFI
