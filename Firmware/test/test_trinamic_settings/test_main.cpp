// Нативные unit-тесты граничных случаев настроек Trinamic.
// Запуск: cd Firmware && pio test -e native
//
// Покрывают три места, где значения настроек раньше приводили к беде:
//   $Axis/Microsteps вне поддерживаемого ряда — молча игнорировался драйвером;
//   microsteps = 0 в calc_tstep() — целочисленное деление на ноль и ребут ESP32;
//   $Axis/Current/Run = 0 — rms_current(0) программировал максимальный ток.

#include <unity.h>

#include "Motors/TrinamicSettings.h"

void setUp(void) {}
void tearDown(void) {}

// --- микрошаг: полный шаг ---
void test_microsteps_full_step(void) {
    TrinamicMicrosteps zero = trinamic_normalize_microsteps(0);
    TEST_ASSERT_TRUE(zero.valid);
    TEST_ASSERT_EQUAL_UINT16(0, zero.value);

    // В grbl 1 — полный шаг, у драйвера это тот же 0
    TrinamicMicrosteps one = trinamic_normalize_microsteps(1);
    TEST_ASSERT_TRUE(one.valid);
    TEST_ASSERT_EQUAL_UINT16(0, one.value);
}

// --- микрошаг: весь поддерживаемый ряд ---
void test_microsteps_supported_values(void) {
    const int32_t supported[] = { 2, 4, 8, 16, 32, 64, 128, 256 };
    for (size_t i = 0; i < sizeof(supported) / sizeof(supported[0]); i++) {
        TrinamicMicrosteps ms = trinamic_normalize_microsteps(supported[i]);
        TEST_ASSERT_TRUE_MESSAGE(ms.valid, "поддерживаемое значение отвергнуто");
        TEST_ASSERT_EQUAL_UINT16((uint16_t)supported[i], ms.value);
    }
}

// --- микрошаг: то, чего TMCStepper не понимает ---
void test_microsteps_rejects_unsupported(void) {
    const int32_t bad[] = { 3, 5, 10, 100, 255, 257, 512, 1000, -1, -16 };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        TrinamicMicrosteps ms = trinamic_normalize_microsteps(bad[i]);
        TEST_ASSERT_FALSE_MESSAGE(ms.valid, "неподдерживаемое значение принято");
    }
}

// --- делитель tstep: ноль не должен ронять контроллер ---
void test_tstep_divisor_survives_zero(void) {
    // 0 = полный шаг, трактуется как 1 -> 256/1
    TEST_ASSERT_EQUAL_UINT16(256, trinamic_tstep_divisor(0));
    TEST_ASSERT_EQUAL_UINT16(256, trinamic_tstep_divisor(1));
}

void test_tstep_divisor_normal_values(void) {
    TEST_ASSERT_EQUAL_UINT16(128, trinamic_tstep_divisor(2));
    TEST_ASSERT_EQUAL_UINT16(16, trinamic_tstep_divisor(16));
    TEST_ASSERT_EQUAL_UINT16(1, trinamic_tstep_divisor(256));
}

// --- ток: ноль означает «выключено», а не «максимум» ---
void test_zero_current_is_off(void) {
    TEST_ASSERT_TRUE(trinamic_current_is_off(0));
}

void test_nonzero_current_is_not_off(void) {
    TEST_ASSERT_FALSE(trinamic_current_is_off(1));
    TEST_ASSERT_FALSE(trinamic_current_is_off(800));
    TEST_ASSERT_FALSE(trinamic_current_is_off(2000));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_microsteps_full_step);
    RUN_TEST(test_microsteps_supported_values);
    RUN_TEST(test_microsteps_rejects_unsupported);
    RUN_TEST(test_tstep_divisor_survives_zero);
    RUN_TEST(test_tstep_divisor_normal_values);
    RUN_TEST(test_zero_current_is_off);
    RUN_TEST(test_nonzero_current_is_not_off);
    return UNITY_END();
}
