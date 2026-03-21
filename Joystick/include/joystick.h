#pragma once

#include <stdint.h>
#include <Arduino.h>

class Joystick
{    
    static const uint8_t X_AXIS_PIN = 1;
    static const uint8_t Y_AXIS_PIN = 0;

    static const int32_t AXIS_MIN_VAL = 0;
    static const int32_t AXIS_MAX_VAL = 2200;

    int32_t read_avg(uint16_t pin, uint32_t read_count = 5)
    {
        int32_t avg = 0;

        for(auto i = 0; i < read_count; i++)
        {
            avg = avg + analogRead(pin);
        }

        return avg / read_count;
    }

    float_t read_axis(uint8_t pin, float_t min, float_t mid, float_t max)
    {
        float_t val = (read_avg(pin) - mid);

        float_t res;

        if(val < 0)
        {
            res = (val - min) / (mid - min);
        }
        else
        {
            res = val / (max - mid);
        }

        return res;
    }

public:

    void setup()
    {
        pinMode(X_AXIS_PIN, ANALOG);
        pinMode(Y_AXIS_PIN, ANALOG);
    }

    bool read(float_t& x, float_t& y)
    {
        x = read_axis(X_AXIS_PIN, AXIS_MIN_VAL, (AXIS_MAX_VAL - AXIS_MIN_VAL) / 2.0F, AXIS_MAX_VAL);
        y = read_axis(Y_AXIS_PIN, AXIS_MIN_VAL, (AXIS_MAX_VAL - AXIS_MIN_VAL) / 2.0F, AXIS_MAX_VAL);
        
        return true;
    }
};

extern Joystick joystick;