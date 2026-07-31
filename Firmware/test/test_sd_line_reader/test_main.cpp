// Нативные unit-тесты разбора строки G-кода: sd_read_line().
// Запуск: cd Firmware && pio test -e native
//
// Именно здесь жили дефекты, из-за которых SD-задание завершалось рапортом об
// успехе посреди файла и писало нулевой байт за границей буфера вызывающего.

#include <string.h>
#include <unity.h>

#include "SDLineReader.h"

void setUp(void) {}
void tearDown(void) {}

// --- источник байтов поверх строки в памяти ---
struct MemSource {
    const char* data;
    size_t      len;
    size_t      pos;
    bool        fail_at_end;  // вместо EOF отдать ошибку чтения
};

static int mem_next(void* ctx) {
    MemSource* src = static_cast<MemSource*>(ctx);
    if (src->pos >= src->len) {
        return src->fail_at_end ? SD_LINE_ERROR : SD_LINE_EOF;
    }
    return (unsigned char)src->data[src->pos++];
}

static SDLineResult read_from(const char* data, char* line, size_t cap, bool fail_at_end = false) {
    MemSource src { data, strlen(data), 0, fail_at_end };
    return sd_read_line(line, cap, mem_next, &src);
}

// --- обычные строки ---
void test_reads_plain_line(void) {
    char         line[64];
    SDLineResult r = read_from("G0 X1\nG0 X2\n", line, sizeof(line));
    TEST_ASSERT_EQUAL(SDLineResult::Line, r);
    TEST_ASSERT_EQUAL_STRING("G0 X1", line);  // '\n' в буфер не попадает
}

void test_reads_empty_line(void) {
    char         line[64];
    SDLineResult r = read_from("\nG0\n", line, sizeof(line));
    TEST_ASSERT_EQUAL(SDLineResult::Line, r);  // пустая строка — валидная строка
    TEST_ASSERT_EQUAL_STRING("", line);
}

void test_reads_last_line_without_newline(void) {
    char         line[64];
    SDLineResult r = read_from("M5", line, sizeof(line));
    TEST_ASSERT_EQUAL(SDLineResult::Line, r);
    TEST_ASSERT_EQUAL_STRING("M5", line);
}

void test_reports_eof_on_empty_source(void) {
    char         line[64];
    SDLineResult r = read_from("", line, sizeof(line));
    TEST_ASSERT_EQUAL(SDLineResult::Eof, r);
    TEST_ASSERT_EQUAL_STRING("", line);
}

// --- граница буфера: тут раньше писался '\0' за массивом ---
void test_accepts_line_that_exactly_fits(void) {
    char line[8];
    // 7 символов + '\n' — ровно cap-1, должно приниматься целиком
    SDLineResult r = read_from("1234567\nX\n", line, sizeof(line));
    TEST_ASSERT_EQUAL(SDLineResult::Line, r);
    TEST_ASSERT_EQUAL_STRING("1234567", line);
}

void test_rejects_line_one_char_too_long(void) {
    char line[8];
    SDLineResult r = read_from("12345678\n", line, sizeof(line));
    TEST_ASSERT_EQUAL(SDLineResult::TooLong, r);
    TEST_ASSERT_EQUAL_STRING("1234567", line);  // буфер терминирован в границах
}

void test_long_last_line_without_newline_does_not_overflow(void) {
    // Ровно cap символов и конец источника: раньше это писало line[cap] = '\0'.
    char guard[16];
    memset(guard, 0x5A, sizeof(guard));
    char*        line = guard;  // «буфер» — первые 8 байт, дальше сторожевая зона
    SDLineResult r    = read_from("12345678", line, 8);
    TEST_ASSERT_EQUAL(SDLineResult::TooLong, r);
    TEST_ASSERT_EQUAL_STRING("1234567", line);
    TEST_ASSERT_EQUAL_HEX8(0x5A, (unsigned char)guard[8]);  // сторож цел
}

// --- ошибки чтения не должны выглядеть как конец файла ---
void test_read_error_is_not_eof(void) {
    char         line[64];
    SDLineResult r = read_from("G0 X1", line, sizeof(line), /*fail_at_end=*/true);
    TEST_ASSERT_EQUAL(SDLineResult::ReadError, r);
}

void test_read_error_on_first_byte(void) {
    char         line[64];
    SDLineResult r = read_from("", line, sizeof(line), /*fail_at_end=*/true);
    TEST_ASSERT_EQUAL(SDLineResult::ReadError, r);
}

// --- вырожденные аргументы ---
void test_rejects_bad_arguments(void) {
    char      line[8];
    MemSource src { "x", 1, 0, false };
    TEST_ASSERT_EQUAL(SDLineResult::ReadError, sd_read_line(nullptr, sizeof(line), mem_next, &src));
    TEST_ASSERT_EQUAL(SDLineResult::ReadError, sd_read_line(line, 0, mem_next, &src));
    TEST_ASSERT_EQUAL(SDLineResult::ReadError, sd_read_line(line, sizeof(line), nullptr, &src));
}

// --- CR остаётся в строке: так же, как было до рефакторинга ---
void test_keeps_carriage_return(void) {
    char         line[64];
    SDLineResult r = read_from("G0\r\n", line, sizeof(line));
    TEST_ASSERT_EQUAL(SDLineResult::Line, r);
    TEST_ASSERT_EQUAL_STRING("G0\r", line);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_reads_plain_line);
    RUN_TEST(test_reads_empty_line);
    RUN_TEST(test_reads_last_line_without_newline);
    RUN_TEST(test_reports_eof_on_empty_source);
    RUN_TEST(test_accepts_line_that_exactly_fits);
    RUN_TEST(test_rejects_line_one_char_too_long);
    RUN_TEST(test_long_last_line_without_newline_does_not_overflow);
    RUN_TEST(test_read_error_is_not_eof);
    RUN_TEST(test_read_error_on_first_byte);
    RUN_TEST(test_rejects_bad_arguments);
    RUN_TEST(test_keeps_carriage_return);
    return UNITY_END();
}
