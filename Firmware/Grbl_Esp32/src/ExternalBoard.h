#pragma once

#include <Adafruit_MPR121.h>
#include "Config.h"
#include "Grbl.h"
#include "Uart.h"

extern Uart ext_board_uart;

#define EXT_BOARD_LOG(str, ...) { \
    char temp[128]; \
    snprintf(temp, sizeof(temp), str, ##__VA_ARGS__); \
    Uart0.printf("EXT BOARD: %s\n", temp); }

class ExternalBoard
{
    uint8_t data[64];
    size_t data_readed;

    enum class ButtonEvent
    {
        Click,
        LongPress
    };

    using button_event_t = std::function<void(ButtonEvent, uint32_t)>;
    
    struct Button
    {
        bool state;
        bool prev_state;
        bool flag;

        uint8_t click_count = 0;
        uint32_t last_press_time_ms = 0;
        uint32_t last_click_time_ms = 0;

        bool long_press_fired = false;
        button_event_t on_event = nullptr;

        uint32_t long_press_time_ms = 1000;
    };

    enum JoystickButton : uint32_t
    {
        JOYSTICK_LEFT,
        JOYSTICK_RIGHT,
        JOYSTICK_UP,
        JOYSTICK_DOWN,

        JOYSTICK_START,
        JOYSTICK_SELECT,

        JOYSTICK_L,
        JOYSTICK_R,

        JOYSTICK_A,
        JOYSTICK_B,
        JOYSTICK_X,
        JOYSTICK_Y,

        JOYSTICK_BUTTON_MAX
    };

    Button buttons[JOYSTICK_BUTTON_MAX];

    static const uint32_t press_click_delay = 500;
    static const uint32_t double_click_delay = 500;
    
    struct __attribute__((packed)) JoystickReport
    {
        uint8_t x; // 0-255, 128 is center
        uint8_t y; // 0-255, 128 is center

        uint8_t unused1, unused2, unused3;

        uint8_t buttons1;// x=0x10 a=0x20 b=0x40 y=0x80
        uint8_t buttons2;// START=0x20 SELECT=0x10 L=0x01 R=0x02

        void get_buttons(Button* buttons)
        {
            for(auto i = 0; i < JOYSTICK_BUTTON_MAX; i++)
            {
                buttons[i].prev_state = buttons[i].state;
                buttons[i].state = false;
                buttons[i].flag = false;
            }

            if(x < 0x7)
                buttons[JOYSTICK_LEFT].state = true;
            
            if(x > 0x7F)
                buttons[JOYSTICK_RIGHT].state = true;

            if(y < 0x7F)
                buttons[JOYSTICK_UP].state = true;

            if(y > 0x7F)
                buttons[JOYSTICK_DOWN].state = true;

            if(buttons2 & 0x01)
                buttons[JOYSTICK_L].state = true;

            if(buttons2 & 0x02)                    
                buttons[JOYSTICK_R].state = true;

            if(buttons2 & 0x10)
                buttons[JOYSTICK_SELECT].state = true;

            if(buttons2 & 0x20)
                buttons[JOYSTICK_START].state = true;

            if(buttons1 & 0x20)
                buttons[JOYSTICK_A].state = true;

            if(buttons1 & 0x40)
                buttons[JOYSTICK_B].state = true;

            if(buttons1 & 0x10)
                buttons[JOYSTICK_X].state = true;

            if(buttons1 & 0x80)
                buttons[JOYSTICK_Y].state = true;
        }
    };

    enum class MoveMode : uint32_t
    {
        mm_0_1 = 10,
        mm_1_0 = 100,
        mm_10_0 = 1000,
        Jog = 50000

    } move_mode = MoveMode::mm_1_0;

    bool is_pressed(JoystickButton btn)
    {
        return buttons[btn].state;
    }

    bool is_released(JoystickButton btn)
    {
        return !buttons[btn].state;
    }

    bool is_new_pressed(JoystickButton btn)
    {
        return buttons[btn].state && !buttons[btn].prev_state;
    }

    bool is_new_released(JoystickButton btn)
    {
        return !buttons[btn].state && buttons[btn].prev_state;
    }

    bool is_changed(JoystickButton btn)
    {
        return buttons[btn].state != buttons[btn].prev_state;
    }

    bool is_click_and_reset(JoystickButton btn, uint8_t click_count)
    {
        if(buttons[btn].click_count == click_count)
        {
            buttons[btn].last_press_time_ms = 0;
            buttons[btn].last_click_time_ms = 0;
            buttons[btn].click_count = 0;
            return true;
        }

        return false;
    }

    static const JoystickButton X_INC = JOYSTICK_RIGHT;
    static const JoystickButton X_DEC = JOYSTICK_LEFT;
    static const JoystickButton Y_INC = JOYSTICK_UP;
    static const JoystickButton Y_DEC = JOYSTICK_DOWN;
    static const JoystickButton Z_INC = JOYSTICK_X;
    static const JoystickButton Z_DEC = JOYSTICK_Y;
    
    static const JoystickButton RESET = JOYSTICK_START;
    static const JoystickButton PROBE = JOYSTICK_SELECT;
    static const JoystickButton HOME = JOYSTICK_B;
    static const JoystickButton ZERO = JOYSTICK_A;

    struct AxisMove
    {
        int8_t x;
        int8_t y;
        int8_t z;
    };

    void get_axis(AxisMove& move);

    void send_gcode(const char* gcode)
    {
        serila_write_into_buffer((uint8_t*)gcode);
    }

public:

    void begin();

    void handle();

    void on_joystick();
    void probe_check_tick();
};

extern ExternalBoard ext_board;

/*
class MPR121_Buttons;

extern MPR121_Buttons mpr121_buttons;

#define MPR121_LOG(str, ...) \
    { \
        char temp[128]; \
        snprintf(temp, sizeof(temp), str, ##__VA_ARGS__); \
        Uart0.printf("MPR121: %s\n", temp); \
    }

class MPR121_Buttons
{
    uint16_t lasttouched = 0;
    uint16_t currtouched = 0;

    Adafruit_MPR121 mpr121 = Adafruit_MPR121();

    enum class Button : uint8_t
    {
        X_INC = 2,
        X_DEC = 10,

        Y_INC = 5,
        Y_DEC = 7,

        Z_INC = 3,
        Z_DEC = 11,


        RESET = 8,
        HOME = 7,
        ZERO = 4,

        MODE_CHANGE = 1,
        FEED_CHANGE = 9,
        PROBE = 0,

        ButtonMax = 12
    };

    enum class MoveMode : uint8_t
    {
        mm_0_1,
        mm_1_0,
        mm_10_0,
        Jog

    } move_mode = MoveMode::mm_1_0;

    enum class MoveFeed : uint16_t
    {
        Slow = 100,
        Fast = 1000

    } move_feed = MoveFeed::Fast;

    float_t move_distance(MoveMode mode)
    {
        switch(mode)
        {
            case MoveMode::mm_0_1:
                return 0.1f;

            case MoveMode::mm_1_0:
                return 1.0f;

            case MoveMode::mm_10_0:
                return 10.0f;
        }

        return 0.0f;
    }

public:

    TaskHandle_t _task;

    bool is_move_button(Button btn)
    {
        switch(btn)
        {
            case Button::X_INC:
            case Button::X_DEC:
            case Button::Y_INC:
            case Button::Y_DEC:
            case Button::Z_INC:
            case Button::Z_DEC:
                return true;
            
            default:
                return false;
        }
    }

    char btn_to_axis(Button btn)
    {
        switch(btn)
        {
            case Button::X_INC:
            case Button::X_DEC:
                return 'X';

            case Button::Y_INC:
            case Button::Y_DEC:
                return 'Y';

            case Button::Z_INC:
            case Button::Z_DEC:
                return 'Z';
            
            default:
                return '\0';
        }
    }

    void send_gcode(const char* gcode)
    {
        serila_write_into_buffer((uint8_t*)gcode);
    }

    void on_button(Button btn, bool pressed)
    {
        MPR121_LOG("Button %d %s", (int)btn, pressed ? "pressed" : "released");

        if(is_move_button(btn))
        {
            char axis = btn_to_axis(btn);

            float_t direction = (btn == Button::X_DEC || btn == Button::Y_DEC || btn == Button::Z_DEC) ? -1.0f : 1.0f;

            if(pressed)
            {
                float_t distance = move_mode == MoveMode::Jog ? 1000.0f :(int32_t)(move_distance(move_mode));
                auto str = "$J=G91" + String(axis) + String(distance) + "F" + String((int)move_feed) + "\n";
                send_gcode(str.c_str());
            }
            else
            {
                char cancel[] = {0x85, 0};
                send_gcode(cancel);
            }
        }
        else if(pressed)
        {
            switch(btn)
            {
                case Button::RESET:
                    {
                        mc_reset();
                        MPR121_LOG("Reset");
                    }
                    break;

                case Button::ZERO:
                    {
                        send_gcode("G92 X0 Y0 Z0\n");
                        MPR121_LOG("Position zeroed");
                    }
                    break;

                case Button::MODE_CHANGE:
                    {
                        switch(move_mode)
                        {
                            case MoveMode::mm_0_1:
                                move_mode = MoveMode::mm_1_0;
                                MPR121_LOG("Move mode: 1.0mm");
                                break;
                            case MoveMode::mm_1_0:
                                move_mode = MoveMode::mm_10_0;
                                MPR121_LOG("Move mode: 10.0mm");
                                break;
                            case MoveMode::mm_10_0:
                                move_mode = MoveMode::Jog;
                                MPR121_LOG("Move mode: Jog");
                                break;
                            case MoveMode::Jog:
                                move_mode = MoveMode::mm_0_1;
                                MPR121_LOG("Move mode: 0.1mm");
                        }
                    }
                    break;
                case Button::FEED_CHANGE:
                    {
                        switch(move_feed)
                        {
                            case MoveFeed::Slow:
                                move_feed = MoveFeed::Fast;
                                MPR121_LOG("Move feed: Fast");
                                break;
                            case MoveFeed::Fast:
                                move_feed = MoveFeed::Slow;
                                MPR121_LOG("Move feed: Slow");
                                break;
                        }
                    }
                case Button::PROBE:
                    {
                        if(pressed)
                        {
                            send_gcode("$J=G91Z-100F200\n");
                            MPR121_LOG("Probe start");
                        }
                        else
                        {
                            char cancel[] = {0x85, 0};
                            send_gcode(cancel);
                            MPR121_LOG("Probe cancel");
                        }
                    }
                    break;
                default:
                    break;
            }
        }
    }

    void irq()
    {
        auto val = mpr121.touched();
        
        if(val != 0xFFF)
            currtouched = val;

        for (uint8_t i = 0; i < (uint8_t)Button::ButtonMax; i++) 
        {
            // it if *is* touched and *wasnt* touched before, alert!
            if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) 
            {
                on_button((Button)(i), true);
            }
            // if it *was* touched and now *isnt*, alert!
            if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) 
            {
                on_button((Button)(i), false);
            }
        }

        lasttouched = currtouched;
    }

    static void task_loop(void* ptr)
    {
        uint32_t pulNotificationValue;

        while(true)
        {
            xTaskNotifyWait(0x0, 0x0, &pulNotificationValue, pdMS_TO_TICKS(10000));

            static_cast<MPR121_Buttons*>(ptr)->irq();
        }
    }

    void mpr121_write(uint8_t reg, uint8_t val)
    {
        mpr121.writeRegister(reg, val);
    }

    void begin()
    {
        Wire.begin(IIC_SDA_PIN , IIC_SCL_PIN);

        if(!mpr121.begin(MPR121_I2CADDR_DEFAULT, &Wire, 50, 25, true))
            grbl_send(CLIENT_ALL, "MPR121 init error\n");

            mpr121_write(0x5E, 0x00);

// 2) Baseline filtering (рекомендованный базовый набор)
mpr121_write(0x2B, 0x01); // MHDR
mpr121_write(0x2C, 0x01); // NHDR
mpr121_write(0x2D, 0x00); // NCLR
mpr121_write(0x2E, 0x00); // FDLR

mpr121_write(0x2F, 0x01); // MHDF
mpr121_write(0x30, 0x01); // NHDF
mpr121_write(0x31, 0xFF); // NCLF
mpr121_write(0x32, 0x02); // FDLF

// 3) Thresholds for all 12 electrodes
for (uint8_t i = 0; i < 12; i++) {
  mpr121_write(0x41 + i * 2, 50); // touch threshold
  mpr121_write(0x42 + i * 2, 25); // release threshold
}

// 4) Debounce: touch=2, release=2
mpr121_write(0x5B, 0x11);

// 5) Filter / timing / charge current
// Более спокойный стартовый вариант
mpr121_write(0x5C, 0x10);
mpr121_write(0x5D, 0x24);

// 6) Auto-config (опционально, хороший старт)
mpr121_write(0x7B, 0x0B);
mpr121_write(0x7D, 0x9C);
mpr121_write(0x7E, 0x65);
mpr121_write(0x7F, 0x8C);

// 7) Enable 12 electrodes
mpr121_write(0x5E, 0x0C);

            //mpr121.setAutoconfig(true);

        //Wire.begin(IIC_SDA_PIN , IIC_SCL_PIN);

        xTaskCreatePinnedToCore(task_loop,     // task
                            "mpr121",         // name for task
                            4098,    // size of task stack
                            this,               // parameters
                            1,      // priority
                            // nullptr,
                            &_task,
                            1);

        #if MPR121_IRQ_PIN != -1
            pinMode(MPR121_IRQ_PIN, INPUT_PULLUP);
            attachInterrupt(MPR121_IRQ_PIN, []() 
            { 
                BaseType_t pxHigherPriorityTaskWoken;
                xTaskNotifyFromISR(mpr121_buttons._task, 0, eNoAction, &pxHigherPriorityTaskWoken); 

            }, CHANGE);
        #endif
    }

    void end()
    {

    }

    void handle()
    {

    }


};*/