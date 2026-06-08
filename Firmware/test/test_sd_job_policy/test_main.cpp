// Нативные unit-тесты чистого классификатора line_safe_during_sd_job().
// Запуск: cd Firmware && pio test -e native
//
// Проверяем политику «SD-печать + команды со второго интерфейса»:
//   read-only запросы и пустые строки -> можно во время задания;
//   любое движение/исполнение/запись настроек -> нельзя.

#include <unity.h>

#include "SDJobPolicy.h"

void setUp(void) {}
void tearDown(void) {}

// --- разрешено во время SD-задания: read-only запросы GRBL ---
void test_allows_readonly_queries(void) {
    TEST_ASSERT_TRUE(line_safe_during_sd_job("$$"));   // настройки
    TEST_ASSERT_TRUE(line_safe_during_sd_job("$#"));   // offsets
    TEST_ASSERT_TRUE(line_safe_during_sd_job("$G"));   // состояние парсера
    TEST_ASSERT_TRUE(line_safe_during_sd_job("$I"));   // build info
    TEST_ASSERT_TRUE(line_safe_during_sd_job("$N"));   // стартовые блоки (просмотр)
    TEST_ASSERT_TRUE(line_safe_during_sd_job("$"));    // краткая справка
}

// --- разрешено: пустое/пробелы/обрамление, регистронезависимость, nullptr ---
void test_allows_empty_and_formatting(void) {
    TEST_ASSERT_TRUE(line_safe_during_sd_job(""));
    TEST_ASSERT_TRUE(line_safe_during_sd_job("   "));
    TEST_ASSERT_TRUE(line_safe_during_sd_job("\r\n"));
    TEST_ASSERT_TRUE(line_safe_during_sd_job("  $$  "));
    TEST_ASSERT_TRUE(line_safe_during_sd_job("$$\r\n"));
    TEST_ASSERT_TRUE(line_safe_during_sd_job("$g"));   // нижний регистр
    TEST_ASSERT_TRUE(line_safe_during_sd_job("$i"));
    TEST_ASSERT_TRUE(line_safe_during_sd_job(nullptr));
}

// --- заблокировано: любое движение/G-M-код ---
void test_blocks_motion_gcode(void) {
    TEST_ASSERT_FALSE(line_safe_during_sd_job("G0 X10"));
    TEST_ASSERT_FALSE(line_safe_during_sd_job("G1 X1 Y2 F100"));
    TEST_ASSERT_FALSE(line_safe_during_sd_job("M3 S1000"));
    TEST_ASSERT_FALSE(line_safe_during_sd_job("X0Y0"));
    TEST_ASSERT_FALSE(line_safe_during_sd_job("g28"));
}

// --- заблокировано: $-команды, меняющие состояние/настройки ---
void test_blocks_state_changing(void) {
    TEST_ASSERT_FALSE(line_safe_during_sd_job("$H"));          // homing
    TEST_ASSERT_FALSE(line_safe_during_sd_job("$X"));          // unlock
    TEST_ASSERT_FALSE(line_safe_during_sd_job("$J=G91 X10"));  // jog (движение!)
    TEST_ASSERT_FALSE(line_safe_during_sd_job("$100=250"));    // запись настройки
    TEST_ASSERT_FALSE(line_safe_during_sd_job("$N0=G54"));     // запись стартового блока
    TEST_ASSERT_FALSE(line_safe_during_sd_job("$RST=$"));      // сброс
}

// --- заблокировано: запрос с аргументами/записью (страховка от ослабления) ---
void test_blocks_query_with_args(void) {
    TEST_ASSERT_FALSE(line_safe_during_sd_job("$Gfoo"));
    TEST_ASSERT_FALSE(line_safe_during_sd_job("$$=1"));
    TEST_ASSERT_FALSE(line_safe_during_sd_job("$# G0"));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_allows_readonly_queries);
    RUN_TEST(test_allows_empty_and_formatting);
    RUN_TEST(test_blocks_motion_gcode);
    RUN_TEST(test_blocks_state_changing);
    RUN_TEST(test_blocks_query_with_args);
    return UNITY_END();
}
