#include "SDJobPolicy.h"

#include <ctype.h>
#include <string.h>

bool line_safe_during_sd_job(const char* line) {
    if (line == nullptr) {
        return true;
    }
    // Пропустить ведущие пробелы/табы.
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    // Пустая строка (или только CR/LF) безвредна.
    if (*line == '\0' || *line == '\r' || *line == '\n') {
        return true;
    }
    // Разрешены только GRBL-запросы, начинающиеся с '$'. Всё остальное
    // (G/M-код, координаты, и т.п.) — это движение/исполнение → отклонить.
    if (line[0] != '$') {
        return false;
    }
    // Bare "$" (краткая справка) — read-only.
    char cmd = (char)toupper((unsigned char)line[1]);
    if (line[1] == '\0' || line[1] == '\r' || line[1] == '\n') {
        return true;
    }
    // Только read-only отчёты. Намеренно НЕ входят:
    //   $H (homing), $X (unlock), $J= (jog), $RST, запись настроек $<n>=...
    switch (cmd) {
        case '$':  // $$  — настройки
        case '#':  // $#  — параметры G-кода (offsets)
        case 'G':  // $G  — состояние парсера
        case 'I':  // $I  — build info
        case 'N':  // $N  — стартовые блоки (просмотр)
            break;
        default:
            return false;
    }
    // После 2-символьной команды допустимы только пробелы/CR/LF.
    // Это отсекает запись (содержащую '=') и любые аргументы: $N0=..., $G x и т.п.
    for (const char* rest = line + 2; *rest != '\0'; rest++) {
        if (*rest != ' ' && *rest != '\t' && *rest != '\r' && *rest != '\n') {
            return false;
        }
    }
    return true;
}
