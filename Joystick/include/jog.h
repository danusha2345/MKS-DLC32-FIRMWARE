#pragma once

#include "joystick.h"
#include "grbl.h"

class Jog
{
    enum class Direction
    {
        None,
        Forward,
        Back
    };

    static inline constexpr char CMD_STOP = 0x85;
    static inline constexpr char CMD_RESET_FEED_RATE = 0x90;
    static inline constexpr char CMD_INCREMENT_FEED_RATE = 0x91;
    static inline constexpr char CMD_DECTENEMT_FEED_RATE = 0x92;

    const float_t max_feed = 1000.0;

    float_t current_feed_rate = 0.0;

    Direction dir_x = Direction::None;
    Direction dir_y = Direction::None;

    String new_jog_cmd;

    Direction calc_joystick_axis(float_t value, float_t& feed_rate)
    {
        auto abs_value = abs(value);
        
        if(abs_value <= 0.2)
            return Direction::None;

        feed_rate = min(1.0F, round(abs_value * 10.0F) / 10.0F);

        return value > 0 ? Direction::None : Direction::Back;
    }

    bool send_command(const char cmd)
    {
        #if GRBL_JOG_DEBUG > 0
            Serial.printf("0x%X\n", (int)cmd);
            return true;
        #else
            return grblParser.sendCommandExpectingOk(std::string(1, cmd));
        #endif
    }

    bool send_command(String cmd)
    {
        #if GRBL_JOG_DEBUG > 0
            Serial.printf("%s\n", cmd.c_str());
            return true;
        #else
            return grblParser.sendCommandExpectingOk(std::string(cmd.c_str()));
        #endif
    }

public:

    void setup()
    {

    }

    void update()
    {
        float_t x, y;

        auto cur_time = millis();

        if(joystick.read(x, y))
        {
            #if GRBL_JOG_DEBUG > 0
                EVERY_N_TIME(coords, 500)
                {
                    Serial.printf("X:%f Y:%f\n", x, y);
                }
            #endif

            float_t feed_rate;
            float_t new_feed_rate = 1.0;

            Direction new_dir_x, new_dir_y;

            if((new_dir_x = calc_joystick_axis(x, feed_rate)) != Direction::None)
            {
                new_feed_rate = min(new_feed_rate, feed_rate);
            }
            
            if((new_dir_y = calc_joystick_axis(y, feed_rate)) != Direction::None)
            {
                new_feed_rate = min(new_feed_rate, feed_rate);
            }

            if(dir_x != new_dir_x || dir_y != new_dir_y)
            {
                dir_x = new_dir_x;
                dir_y = new_dir_y;

                if(new_dir_x == Direction::None && new_dir_y == Direction::None)
                {
                    send_command(CMD_STOP);
                }
                else
                {
                    String cmd = String("$J=G91") +
                        (new_dir_x == Direction::None ? "" : ((new_dir_x == Direction::Forward ? " X100" : " X-100"))) +
                        (new_dir_y == Direction::None ? "" : ((new_dir_y == Direction::Forward ? " Y100" : " Y-100"))) +
                            " F" + round(max_feed * new_feed_rate) + "\n";
                    
                    current_feed_rate = new_feed_rate;

                    send_command(CMD_STOP);
                    send_command(CMD_RESET_FEED_RATE);

                    send_command(cmd);
                }
            }
            else
            {
                if(abs(current_feed_rate - new_feed_rate) > 0.1)
                {
                    if(current_feed_rate < new_feed_rate)
                    {
                        send_command(CMD_INCREMENT_FEED_RATE);
                        current_feed_rate *= 1.1;
                    }
                    else
                    {
                        send_command(CMD_DECTENEMT_FEED_RATE);
                        current_feed_rate /= 1.1;
                    }
                }
            }
        }
    }
};

extern Jog jog;