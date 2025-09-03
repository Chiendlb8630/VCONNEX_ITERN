#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "iic_sw.h"

#define I2C_MASTER_SCL_IO           22
#define I2C_MASTER_SDA_IO           21
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          100000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_TIMEOUT_MS              100

#define IIC_ADDR 0x37
#define TOUCH_REG 0xAA
#define LED_RMT_RES_HZ (10 * 1000 * 1000) 

static const char *TAG = "I2C_TOUCH_DRIVER";

esp_err_t i2c_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 
                               I2C_MASTER_RX_BUF_DISABLE, 
                               I2C_MASTER_TX_BUF_DISABLE, 0);
}

esp_err_t Read_touch(uint8_t *touch_state){
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IIC_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, TOUCH_REG, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (IIC_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, touch_state, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return ret;
}




