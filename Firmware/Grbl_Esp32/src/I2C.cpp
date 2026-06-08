#include "Config.h"   // подтянуть мастер-флаг ENABLE_EXTERNAL_BOARD
#ifdef ENABLE_EXTERNAL_BOARD

#include "I2C.h"
#include "driver/gpio.h"
#include "driver/i2c.h"

#define I2C_DEV_ADDR 0x55

Uart Uart1(1);

void i2c_init()
{
    /*uint8_t rx_data[5];

	i2c_config_t conf;
    
    conf.mode = I2C_MODE_MASTER;
	conf.sda_io_num = IIC_SDA;
	conf.scl_io_num = IIC_SCL;
	conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
	conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
	conf.master.clk_speed = 100000;
	
    i2c_param_config(I2C_NUM_0, &conf);

    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);

    //xTaskCreate(&hello_task, "blinky", 512,NULL,5,NULL );*/

    Uart1.setPins(IIC_SDA, IIC_SCL);  // Tx 1, Rx 3 - standard hardware pins
    Uart1.begin(BAUD_RATE, Uart::Data::Bits8, Uart::Stop::Bits1, Uart::Parity::None);

    //client_reset_read_buffer(CLIENT_ALL);
    // Uart0.write("\r\n");  // create some white space after ESP32 boot info
    //Uart1.write("\n");  // create some white space after ESP32 boot info

    //Serial.begin(115200, SERIAL_8N1, IIC_SCL, IIC_SDA);
}

void i2c_loop()
{
}

int i2c_read()
{
    return Uart1.read();

    /*uint8_t byte = 0;
    i2c_cmd_handle_t OurI2cCmdHandle = i2c_cmd_link_create();

	i2c_master_start(OurI2cCmdHandle);

	i2c_master_write_byte(OurI2cCmdHandle, (I2C_DEV_ADDR << 0x1) | I2C_MASTER_WRITE, true);
	i2c_master_read_byte(OurI2cCmdHandle, &byte, I2C_MASTER_ACK);
    
	i2c_master_stop(OurI2cCmdHandle);
	//Send queued commands
	auto ret = i2c_master_cmd_begin(I2C_NUM_0, OurI2cCmdHandle, 1 / portTICK_RATE_MS);

	i2c_cmd_link_delete(OurI2cCmdHandle);

    return ret == ESP_OK ? byte : -1;*/
}

void i2c_write(const char* text)
{
    /*i2c_cmd_handle_t OurI2cCmdHandle = i2c_cmd_link_create();

    i2c_master_start(OurI2cCmdHandle);
	i2c_master_write_byte(OurI2cCmdHandle, (I2C_DEV_ADDR << 0x1) | I2C_MASTER_WRITE, 1);

    //for(auto i = 0; i < size; i++)
	    i2c_master_write(OurI2cCmdHandle, (uint8_t*)buffer, size, true);

	i2c_master_stop(OurI2cCmdHandle);

	auto err = i2c_master_cmd_begin(I2C_NUM_0, OurI2cCmdHandle, 1 / portTICK_RATE_MS);

    i2c_cmd_link_delete(OurI2cCmdHandle);

    //Uart0.printf("i2c: %i\n", (int)0);*/

    Uart1.write(text);
}

int i2c_available()
{
    return 1;
}

#endif // ENABLE_EXTERNAL_BOARD