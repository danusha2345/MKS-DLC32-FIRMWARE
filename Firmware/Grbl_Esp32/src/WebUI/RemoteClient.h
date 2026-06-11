#pragma once

/*
  RemoteClient.h - исходящее подключение станка к удалённому серверу управления

  Плата сама устанавливает TCP-соединение с сервером (адрес задаётся настройкой
  $Remote/Server, формат host:port), при подключении отправляет авторизационную
  строку с токеном станка ($Remote/Token) и MAC-адресом, после чего канал
  работает как обычная telnet-сессия GRBL: сервер шлёт команды, плата отвечает.

  При обрыве связи подключение восстанавливается автоматически.
*/

#include "../Config.h"

namespace WebUI {
    class Remote_Client {
    public:
        static void begin();
        static void end();
        static void handle();
    };
}
