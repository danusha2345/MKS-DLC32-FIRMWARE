#include "Config.h"   // подтянуть мастер-флаг ENABLE_EXTERNAL_BOARD
#ifdef ENABLE_EXTERNAL_BOARD

#include "ExternalBoard.h"
#include "mks/MKS_draw_move.h"

Uart ext_board_uart = Uart(UART_NUM_1);

#define UART_WRITE_BYTES(DATA, SIZE) \
    ext_board_uart.write((uint8_t*)DATA, SIZE)

#define UART_READ_BYTES(DATA, SIZE, TIMEOUT) \
    ext_board_uart.readBytes((uint8_t*)DATA, SIZE, TIMEOUT)

#define UART_AVAIL() \
    ext_board_uart.available()

#define EB_UART_CMD_LOG(str, ...) EXT_BOARD_LOG(str, __VA_ARGS__)

#include "../../../ExternalBoard/Firmware/App/main/uart_cmd.h"

EB_UART_CMD cmd;

void ExternalBoard::get_axis(AxisMove& move)
{
    move.x = move.y = move.z = 0;

    if(is_pressed(X_INC))
        move.x = 1;
    
    if(is_pressed(X_DEC))
        move.x = -1;

    if(is_pressed(Y_INC))
        move.y = 1;

    if(is_pressed(Y_DEC))
        move.y = -1;

    if(is_pressed(Z_INC))
        move.z = 1;

    if(is_pressed(Z_DEC))
        move.z = -1;
}   

#define ANY_MOVE_BUTTONS(func) \
    func(X_INC) || func(X_DEC) || func(Y_INC) || func(Y_DEC) || func(Z_INC) || func(Z_DEC)

void ExternalBoard::on_joystick()
{    
    auto prev_move_mode = move_mode;

    if(is_pressed(JOYSTICK_L) && is_pressed(JOYSTICK_R))
    {
        move_mode = MoveMode::Jog;
    }
    else if(is_pressed(JOYSTICK_L))
    {
        move_mode = MoveMode::mm_0_1;
    }
    else if(is_pressed(JOYSTICK_R))
    {
        move_mode = MoveMode::mm_10_0;
    }
    else
    {
        move_mode = MoveMode::mm_1_0;
    }

    bool new_move = false;

    if(move_mode == MoveMode::Jog)
    {
        if(ANY_MOVE_BUTTONS(is_changed))
        {
            new_move = true;

            char cancel[] = {0x85, 0};
            send_gcode(cancel);
            delay(200);
        }
    }
    else
    {
        if(prev_move_mode == MoveMode::Jog)
        {
            char cancel[] = {0x85, 0};
            send_gcode(cancel);
            delay(200);
        }

        if(ANY_MOVE_BUTTONS(is_new_pressed))
        {
            new_move = true;
        }
    }

    if(new_move)
    {
        AxisMove move;
        get_axis(move);

        if(move.x != 0 || move.y != 0 || move.z != 0)
        {
            EXT_BOARD_LOG("Move: x=%d y=%d z=%d", move.x, move.y, move.z);
            
            float_t move_distance = ((float_t)move_mode / 100.0f);

            auto str = String("$J=G91") +

                (move.x != 0 ? (String(" X") + String(move_distance * (float_t)move.x)) : "") +
                (move.y != 0 ? (String(" Y") + String(move_distance * (float_t)move.y)) : "") +
                (move.z != 0 ? (String(" Z") + String(move_distance * (float_t)move.z)) : "") +

                " F" + String(500) + "\n";

            send_gcode(str.c_str());
        }
    }

    if(is_new_pressed(RESET))
    {
        mc_reset();
        EXT_BOARD_LOG("RESET");
        beep_time(200);
    }

}

void ExternalBoard::begin()
{
    ext_board_uart.setPins(EXT_BOARD_TX_PIN, EXT_BOARD_RX_PIN);
    ext_board_uart.begin(115200, Uart::Data::Bits8, Uart::Stop::Bits1, Uart::Parity::None);

    buttons[ZERO].long_press_time_ms = 1000;

    buttons[ZERO].on_event = [this](ButtonEvent event, uint32_t click_count)
    {
        if(event == ButtonEvent::LongPress)
        {
            send_gcode("G92 X0 Y0 Z0\n");
            EXT_BOARD_LOG("ZERO");
            beep_time(200);
        }
    };

    buttons[HOME].long_press_time_ms = 1000;

    buttons[HOME].on_event = [this](ButtonEvent event, uint32_t click_count)
    {
        if(event == ButtonEvent::LongPress)
        {
            send_gcode("G28\n");
            EXT_BOARD_LOG("HOME");
            beep_time(200);
        }
    };

    buttons[PROBE].long_press_time_ms = 1000;

    buttons[PROBE].on_event = [this](ButtonEvent event, uint32_t click_count)
    {
        set_knife1();

        EXT_BOARD_LOG("PROBE %d : %d", event, click_count);

        beep_time(200);
    };
}

void ExternalBoard::handle()
{
    while(true)
    {
        auto res = cmd.read_cmd();

        if(res == EB_UART_CMD::RESULT_DONE)
        {
            if(cmd.header.cmd == EB_UART_CMD::CMD_INPUT)
            {
                if(cmd.data_size >= sizeof(JoystickReport))
                {
                    auto report = (JoystickReport*)cmd.data;

                    report->get_buttons(buttons);

                    uint32_t now = millis();

                    for(auto i = 0; i < JOYSTICK_BUTTON_MAX; i++)
                    {
                        auto button = &buttons[i];

                        if(!button->on_event)
                            continue;

                        if(is_new_pressed((JoystickButton)(i)))
                        {
                            button->last_press_time_ms = now;
                            button->long_press_fired = false;
                        }
                        else if(is_new_released((JoystickButton)(i)))
                        {
                            if((now - button->last_press_time_ms) <= press_click_delay)
                            {
                                button->click_count++;
                                
                                button->last_click_time_ms = now;
                            }
                            else
                            {
                                button->click_count = 0;
                                button->last_click_time_ms = 0;
                            }
                        }
                    }

                    on_joystick();
                }
            }
        }
        else if(res == EB_UART_CMD::RESULT_CONTINUE)
        {
            continue;
        }
        else if(res == EB_UART_CMD::RESULT_ERROR)
        {
            EXT_BOARD_LOG("Error reading command");
            break;
        }
        else
        {
            break;
        }
    }

    for(auto i = 0; i < JOYSTICK_BUTTON_MAX; i++)
    {
        auto button = &buttons[i];
        
        if(!button->on_event)
            continue;
        
        if(is_pressed((JoystickButton)(i)) && !button->long_press_fired && button->long_press_time_ms)
        {
            if(millis() - button->last_press_time_ms > button->long_press_time_ms)
            {
                button->on_event(ButtonEvent::LongPress, button->long_press_time_ms);
                button->long_press_fired = true;
                button->click_count = 0;

                EXT_BOARD_LOG("Btn %d LongPress", i);
            }
        }

        if(is_new_released((JoystickButton)(i)) && button->click_count && !button->long_press_fired)
        {
            if(millis() - button->last_click_time_ms > double_click_delay)
            {
                button->on_event(ButtonEvent::Click, button->click_count);

                EXT_BOARD_LOG("Btn %d: click %d", i, button->click_count);

                button->click_count = 0;
            }
        }
    }

    if(beep_end_ms && millis() > beep_end_ms)
    {
        beep(false);
        beep_end_ms = 0;
    }
}

#endif // ENABLE_EXTERNAL_BOARD
