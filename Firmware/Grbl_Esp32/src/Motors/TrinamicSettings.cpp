#include "TrinamicSettings.h"

TrinamicMicrosteps trinamic_normalize_microsteps(int32_t raw) {
    TrinamicMicrosteps result { 0, false };

    if (raw == 0 || raw == 1) {
        result.value = 0;  // полный шаг
        result.valid = true;
        return result;
    }

    if (raw > 1 && raw <= 256 && (raw & (raw - 1)) == 0) {
        result.value = (uint16_t)raw;
        result.valid = true;
        return result;
    }

    // Невалидное значение возвращаем как есть — вызывающий печатает его в предупреждении.
    result.value = (raw > 0 && raw <= 0xFFFF) ? (uint16_t)raw : 0;
    return result;
}

uint16_t trinamic_tstep_divisor(uint16_t microsteps) {
    return (uint16_t)(256 / (microsteps == 0 ? 1 : microsteps));
}

bool trinamic_current_is_off(uint16_t run_current_ma) {
    return run_current_ma == 0;
}
