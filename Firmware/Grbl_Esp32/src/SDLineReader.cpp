#include "SDLineReader.h"

SDLineResult sd_read_line(char* line, size_t cap, sd_byte_source next, void* ctx) {
    if (line == nullptr || cap == 0 || next == nullptr) {
        return SDLineResult::ReadError;
    }

    size_t len = 0;
    for (;;) {
        int ch = next(ctx);
        if (ch == SD_LINE_ERROR) {
            line[len] = '\0';
            return SDLineResult::ReadError;
        }
        if (ch == SD_LINE_EOF) {
            break;
        }

        char c = (char)ch;
        if (c == '\n') {
            line[len] = '\0';
            return SDLineResult::Line;
        }
        // Место под терминатор резервируется здесь, а не после цикла: иначе строка
        // длиной ровно cap записывала бы '\0' за границей буфера.
        if (len + 1 >= cap) {
            line[cap - 1] = '\0';
            return SDLineResult::TooLong;
        }
        line[len++] = c;
    }

    line[len] = '\0';
    // Последняя строка без завершающего перевода строки — это тоже строка.
    return len ? SDLineResult::Line : SDLineResult::Eof;
}
