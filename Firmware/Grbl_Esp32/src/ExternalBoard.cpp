#include "ExternalBoard.h"

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

void ExternalBoard::on_joystick(const JoystickReport* report)
{
    EXT_BOARD_LOG("report %i: x1=%d y1=%d rz1=%d x2=%d y2=%d rz2=%d buttons=%02X %02X",
        cmd.header.cmd,
        report->x1, report->y1, report->rz1,
        report->x2, report->y2, report->rz2,
        report->buttons1, 
        report->buttons2);

    if(report->is_pressed(BUTTON::UP))
    {
        EXT_BOARD_LOG("Button A is pressed");
    }
    else
    {
        EXT_BOARD_LOG("Button A is released");
    }
}

void ExternalBoard::handle()
{
    while(true)
    {
        auto res = cmd.read_cmd();

        if(res == EB_UART_CMD::RESULT_DONE)
        {
            on_joystick((JoystickReport*)cmd.data);
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
}