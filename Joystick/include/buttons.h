#pragma once

#include <stdint.h>
#include <Arduino.h>
#include <uButton.h>

class Buttons
{
public:

    struct Button
    {
        uint8_t pin;
        uButton button;
        char key;
        

        Button(uint8_t pin, char key) : pin(pin), button(uButton(pin)), key(key) 
        {

        }
    };

    Button buttons[7] = 
    {
        Button(5, 'A'),
        Button(20, 'B'),
        Button(10, 'C'),
        Button(9, 'D'),
        Button(8, 'E'),
        Button(7, 'F'),
        Button(6, 'K')
    };

    void setup()
    {
        for(auto& button : buttons)
        {
            attachInterruptArg(digitalPinToInterrupt(button.pin), [](void* arg) 
            {
            //static_cast<Button*>(arg)->button.pressISR();

            }, &button, FALLING);
        }
    }
};