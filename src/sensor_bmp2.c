#include <string.h>
#include "hardware/i2c.h"
#include "bmp2.h"
#include "sensor_bmp2.h"

static const int dev_addr = 0x77;
static struct bmp2_dev dev;
static struct bmp2_config conf;
static absolute_time_t time_wait;

static BMP2_INTF_RET_TYPE bmp2_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    i2c_inst_t *i2c = intf_ptr;
    if (i2c_write_blocking(i2c, dev_addr, &reg_addr, 1, true) != 1)
        return -__LINE__;
    if (i2c_read_blocking(i2c, dev_addr, reg_data, length, false) != (int)length)
        return -__LINE__;
    return 0;
}

static BMP2_INTF_RET_TYPE bmp2_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    uint8_t data[length + 1];
    data[0] = reg_addr;
    memcpy(&data[1], reg_data, length);

    i2c_inst_t *i2c = intf_ptr;
    if (i2c_write_blocking(i2c, dev_addr, data, length + 1, false) != (int)(length + 1))
        return -__LINE__;
    return 0;
}

static void bmp2_delay_us(uint32_t period, __unused void *intf_ptr)
{
    sleep_us(period);
}

int sensor_bmp2_init(i2c_inst_t *i2c)
{
    dev.read = &bmp2_i2c_read;
    dev.write = &bmp2_i2c_write;
    dev.delay_us = &bmp2_delay_us;
    dev.intf = BMP2_I2C_INTF;
    dev.intf_ptr = i2c;

    if (bmp2_init(&dev) != BMP2_OK)
        return -__LINE__;

    // Always read the current settings before writing, especially when all the configuration is not modified
    if (bmp2_get_config(&conf, &dev) != BMP2_OK)
        return -__LINE__;

    conf.filter = BMP2_FILTER_COEFF_16;
    conf.os_mode = BMP2_OS_MODE_ULTRA_HIGH_RESOLUTION;
    conf.odr = BMP2_ODR_0_5_MS;
    if (bmp2_set_config(&conf, &dev) != BMP2_OK)
        return -__LINE__;
    return 0;
}

int sensor_bmp2_start_measurement(void)
{
    if (bmp2_set_power_mode(BMP2_POWERMODE_FORCED, &conf, &dev) != BMP2_OK)
        return -__LINE__;

    absolute_time_t time = get_absolute_time();
    uint32_t meas_time;
    if (bmp2_compute_meas_time(&meas_time, &conf, &dev) != BMP2_OK)
        return -__LINE__;

    time_wait = time + meas_time;
    return 0;
}

int sensor_bmp2_get_measurement(bmp2_result_t *result)
{
    sleep_until(time_wait);

    struct bmp2_status status;
    do {
        if (bmp2_get_status(&status, &dev) != BMP2_OK)
            return -__LINE__;
    } while (status.measuring != BMP2_MEAS_DONE);

    struct bmp2_data comp_data;
    if (bmp2_get_sensor_data(&comp_data, &dev) != BMP2_OK)
        return -__LINE__;

    result->pressure = comp_data.pressure;
    result->temperature = comp_data.temperature;
    return 0;
}
