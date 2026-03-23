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
    WiFiServer*   Telnet_Server::_telnetserver = NULL;

    #ifdef ENABLE_TELNET_OTHER_TASK
        TaskHandle_t _telnet_task;
    #endif

    #define CLIENT_TELNET_INDEX(client) (client - CLIENT_TELNET_MIN)
    #define CLIENT_TELNET_VAL(i) (i + CLIENT_TELNET_MIN)

    void Telnet_Server::begin_all()
    {
        if (telnet_enable->get() == 0) {
            return;
        }

        _port = telnet_port->get();

        //create instance
        _telnetserver = new WiFiServer(_port, TELNET_CLIENTS_TOTAL);
        _telnetserver->setNoDelay(true);
        
        String s = "[MSG:TELNET Started " + String(_port) + "]\r\n";
        grbl_send(CLIENT_ALL, (char*)s.c_str());
        //start telnet server
        _telnetserver->begin();

        for(auto i = 0; i < TELNET_CLIENTS_TOTAL; i++)
            telnet_server[i].begin(i);
            
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
        if(_telnet_task)
        {
            vTaskDelete(_telnet_task);
            _telnet_task = NULL;
        }

        if (_telnetserver) 
        {
            delete _telnetserver;
            _telnetserver = NULL;
        }

        for(auto i = 0; i < TELNET_CLIENTS_TOTAL; i++)
            telnet_server[i].end();
    }

    void Telnet_Server::handle_all()
    {

    }

    void Telnet_Server::_handle_all_real()
    {
        if(_telnetserver == NULL)
            return;

        bool client_found = false;
        
        if (_telnetserver->hasClient()) 
        {
            for(auto i = 0; i < TELNET_CLIENTS_TOTAL; i++)
            {
                //find free/disconnected spot
                if (!telnet_server[i].is_connected()) 
                {
                    auto client = _telnetserver->available();
                    telnet_server[i].setup_client(client);

                    String s = "[MSG:TELNET Connected i:" + String(i) + "]\r\n";
                    grbl_send(CLIENT_ALL, (char*)s.c_str());
                    client_found = true;
                    break;
                }
            }

            if(!client_found)
                _telnetserver->available().stop();
        }
        
        #ifndef ENABLE_TELNET_OTHER_TASK
            for(auto i = 0; i < TELNET_CLIENTS_TOTAL; i++)
                telnet_server[i].handle();
        #endif
    }

    void Telnet_Server::write(uint8_t client, const uint8_t* buffer, size_t size)
    {
        uint8_t index = CLIENT_TELNET_INDEX(client);

        if(index >= 0 && index < TELNET_CLIENTS_TOTAL)
            telnet_server[index].write(buffer, size);
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

    Telnet_Server::Telnet_Server() {
        _RXbufferSize = 0;
        _RXbufferpos  = 0;
    }

    bool Telnet_Server::begin(uint8_t client_index) {
        bool no_error = true;
        end();
        _RXbufferSize = 0;
        _RXbufferpos  = 0;

        _client_index = client_index;
        _setupdone = true;
    }

    void Telnet_Server::end() {
        _setupdone    = false;
        _RXbufferSize = 0;
        _RXbufferpos  = 0;
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
            wsize = _telnetClient.write(buffer, size);
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
            if (_telnetClientIP != _telnetClient.remoteIP()) 
            {
                report_init_message(CLIENT_TELNET_VAL(_client_index));
                _telnetClientIP = _telnetClient.remoteIP();
            }
#    endif
            if (_telnetClient.available()) 
            {
                uint8_t buf[1024];
                COMMANDS::wait(0);
                int readlen  = _telnetClient.available();
                int writelen = TELNETRXBUFFERSIZE - available();
                if (readlen > 1024) {
                    readlen = 1024;
                }
                if (readlen > writelen) {
                    readlen = writelen;
                }
                if (readlen > 0) {
                    _telnetClient.read(buf, readlen);
                    push(buf, readlen);
                }
                return;
            }
        } 
        else 
        {
            if (_telnetClient) 
            {
#    ifdef ENABLE_TELNET_WELCOME_MSG
                _telnetClientIP = IPAddress(0, 0, 0, 0);
#    endif
                _telnetClient.stop();
            }
        }
            
        COMMANDS::wait(0);
    }

    int Telnet_Server::peek(void) {
        if (_RXbufferSize > 0) {
            return _RXbuffer[_RXbufferpos];
        } else {
            return -1;
        }
    }

    int Telnet_Server::available() { return _RXbufferSize; }

    int Telnet_Server::get_rx_buffer_available() { return TELNETRXBUFFERSIZE - _RXbufferSize; }

    bool Telnet_Server::push(uint8_t data) {
        log_i("[TELNET]push %c", data);
        if ((1 + _RXbufferSize) <= TELNETRXBUFFERSIZE) {
            int current = _RXbufferpos + _RXbufferSize;
            if (current > TELNETRXBUFFERSIZE) {
                current = current - TELNETRXBUFFERSIZE;
            }
            if (current > (TELNETRXBUFFERSIZE - 1)) {
                current = 0;
            }
            _RXbuffer[current] = data;
            _RXbufferSize++;
            log_i("[TELNET]buffer size %d", _RXbufferSize);
            return true;
        }
        return false;
    }

    bool Telnet_Server::push(const uint8_t* data, int data_size) {
        if ((data_size + _RXbufferSize) <= TELNETRXBUFFERSIZE) {
            int data_processed = 0;
            int current        = _RXbufferpos + _RXbufferSize;
            if (current > TELNETRXBUFFERSIZE) {
                current = current - TELNETRXBUFFERSIZE;
            }
            for (int i = 0; i < data_size; i++) {
                if (current > (TELNETRXBUFFERSIZE - 1)) {
                    current = 0;
                }

                _RXbuffer[current] = data[i];
                current++;
                data_processed++;

                COMMANDS::wait(0);
                //vTaskDelay(1 / portTICK_RATE_MS);  // Yield to other tasks
            }
            _RXbufferSize += data_processed;
            return true;
        }
        return false;
    }

    int Telnet_Server::read(void) {
        if (_RXbufferSize > 0) {
            int v = _RXbuffer[_RXbufferpos];
            //log_d("[TELNET]read %c",char(v));
            _RXbufferpos++;
            if (_RXbufferpos > (TELNETRXBUFFERSIZE - 1)) {
                _RXbufferpos = 0;
            }
            _RXbufferSize--;
            return v;
        } else {
            return -1;
        }
    }

    Telnet_Server::~Telnet_Server() { end(); }
}
#endif  // Enable TELNET && ENABLE_WIFI
