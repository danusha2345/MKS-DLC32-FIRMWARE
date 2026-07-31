#pragma once

/*
    TrinamicSettings.h — граничные случаи настроек Trinamic-драйверов.

    Вынесено из TrinamicDriver.cpp / TrinamicUartDriverClass.cpp, чтобы проверять
    нативными тестами (test/test_trinamic_settings) без драйвера и TMCStepper.
    Здесь только арифметика, никаких зависимостей от железа.
*/

#include <stdint.h>

// Результат разбора $Axis/Microsteps.
struct TrinamicMicrosteps {
    uint16_t value;  // что писать в драйвер: 0 = полный шаг, иначе степень двойки 2…256
    bool     valid;  // false -> значение драйвер не понимает, регистр трогать нельзя
};

// TMCStepper::microsteps() понимает только 0 и степени двойки до 256, остальное
// молча игнорирует. В grbl 1 означает полный шаг — приводим его к 0.
TrinamicMicrosteps trinamic_normalize_microsteps(int32_t raw);

// Делитель для calc_tstep(): 256 / microsteps. Ноль (полный шаг) — допустимое
// значение настройки, а целочисленное деление на него роняет ESP32
// (IntegerDivideByZero) при StallGuard-хоминге.
uint16_t trinamic_tstep_divisor(uint16_t microsteps);

// Ток 0 мА нельзя отдавать в rms_current(): формула даёт CS = -1, что как uint8_t
// становится 255 и обрезается до 31 — максимальный ток вместо выключенного.
bool trinamic_current_is_off(uint16_t run_current_ma);
