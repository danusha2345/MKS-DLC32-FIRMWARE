/*
  Serial2Socket.cpp -  serial 2 socket functions class

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

#if defined(ENABLE_WIFI) && defined(ENABLE_HTTP)

#    include "Serial2Socket.h"
#    include "WebServer.h"
#    include <WebSocketsServer.h>
#    include <WiFi.h>

namespace WebUI {
    Serial_2_Socket Serial2Socket;

    // broadcastBIN не реентерабелен, а write() вызывается из нескольких задач
    // (protocol loop через grbl_send и clientCheckTask). Доступ к TX-буферу сериализуем
    // коротким спинлоком; саму отправку делает ТОЛЬКО clientCheckTask (handle_flush).
    static portMUX_TYPE s2s_tx_mux = portMUX_INITIALIZER_UNLOCKED;

    Serial_2_Socket::Serial_2_Socket() {
        _web_socket   = NULL;
        _TXbufferSize = 0;
        _RXbufferSize = 0;
        _RXbufferpos  = 0;
    }

    void Serial_2_Socket::begin(long speed) {
        _TXbufferSize = 0;
        _RXbufferSize = 0;
        _RXbufferpos  = 0;
    }

    void Serial_2_Socket::end() {
        _TXbufferSize = 0;
        _RXbufferSize = 0;
        _RXbufferpos  = 0;
    }

    long Serial_2_Socket::baudRate() { return 0; }

    bool Serial_2_Socket::attachWS(WebSocketsServer* web_socket) {
        if (web_socket) {
            _web_socket   = web_socket;
            _TXbufferSize = 0;
            return true;
        }
        return false;
    }

    bool Serial_2_Socket::detachWS() {
        _web_socket = NULL;
        return true;
    }

    Serial_2_Socket::operator bool() const { return true; }

    int Serial_2_Socket::available() { return _RXbufferSize; }

    size_t Serial_2_Socket::write(uint8_t c) {
        if (!_web_socket) {
            return 0;
        }
        write(&c, 1);
        return 1;
    }

    size_t Serial_2_Socket::write(const uint8_t* buffer, size_t size) {
        if ((buffer == NULL) || (!_web_socket)) {
            if (buffer == NULL) {
                log_i("[SOCKET]No buffer");
            }
            if (!_web_socket) {
                log_i("[SOCKET]No socket");
            }
            return 0;
        }

#    if defined(ENABLE_SERIAL2SOCKET_OUT)
        // Только аппенд под спинлоком; flush()/broadcastBIN выполняет clientCheckTask
        // через handle_flush(), чтобы сокет не дёргался из двух задач. При переполнении
        // лишние байты отбрасываем (это консольный вывод) — без записи за границу буфера.
        portENTER_CRITICAL(&s2s_tx_mux);
        if (_TXbufferSize == 0) {
            _lastflush = millis();
        }
        for (size_t i = 0; i < size && _TXbufferSize < TXBUFFERSIZE; i++) {
            _TXbuffer[_TXbufferSize++] = buffer[i];
        }
        portEXIT_CRITICAL(&s2s_tx_mux);
#    endif
        return size;
    }

    int Serial_2_Socket::peek(void) {
        if (_RXbufferSize > 0) {
            return _RXbuffer[_RXbufferpos];
        } else {
            return -1;
        }
    }

    bool Serial_2_Socket::push(const char* data) {
#    if defined(ENABLE_SERIAL2SOCKET_IN)
        int data_size = strlen(data);
        if ((data_size + _RXbufferSize) <= RXBUFFERSIZE) {
            int current = _RXbufferpos + _RXbufferSize;
            if (current > RXBUFFERSIZE) {
                current = current - RXBUFFERSIZE;
            }

            for (int i = 0; i < data_size; i++) {
                if (current > (RXBUFFERSIZE - 1)) {
                    current = 0;
                }
                _RXbuffer[current] = data[i];
                current++;
            }

            _RXbufferSize += strlen(data);
            return true;
        }
        return false;
#    else
        return true;
#    endif
    }

    int Serial_2_Socket::read(void) {
        if (_RXbufferSize > 0) {
            int v = _RXbuffer[_RXbufferpos];
            _RXbufferpos++;

            if (_RXbufferpos > (RXBUFFERSIZE - 1)) {
                _RXbufferpos = 0;
            }
            _RXbufferSize--;
            return v;
        } else {
            return -1;
        }
    }

    void Serial_2_Socket::handle_flush() {
        if (_TXbufferSize > 0 && ((_TXbufferSize >= TXBUFFERSIZE) || ((millis() - _lastflush) > FLUSHTIMEOUT))) {
            log_i("[SOCKET]need flush, buffer size %d", _TXbufferSize);
            flush();
        }
    }
    void Serial_2_Socket::flush(void) {
        // flush() выполняется только в clientCheckTask -> static-буфер безопасен и не
        // нагружает стек. Копируем под спинлоком, broadcastBIN — уже вне критической секции.
        static uint8_t local[TXBUFFERSIZE];
        uint16_t       n = 0;
        portENTER_CRITICAL(&s2s_tx_mux);
        n = _TXbufferSize;
        if (n) {
            memcpy(local, _TXbuffer, n);
            _TXbufferSize = 0;
            _lastflush    = millis();
        }
        portEXIT_CRITICAL(&s2s_tx_mux);
        if (n && _web_socket) {
            _web_socket->broadcastBIN(local, n);
        }
    }

    Serial_2_Socket::~Serial_2_Socket() {
        if (_web_socket) {
            detachWS();
        }
        _TXbufferSize = 0;
        _RXbufferSize = 0;
        _RXbufferpos  = 0;
    }
}
#endif  // ENABLE_WIFI
