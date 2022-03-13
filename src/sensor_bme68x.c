#include <string.h>
#include <math.h>
#include "hardware/i2c.h"
#include "sensor_bme68x.h"
#include "bme68x.h"

static const int dev_addr = 0x76;
static struct bme68x_dev dev;
static struct bme68x_conf conf;
static struct bme68x_heatr_conf heatr_conf;
static absolute_time_t time_wait;

static BME68X_INTF_RET_TYPE bme68x_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    i2c_inst_t *i2c = intf_ptr;
    if (i2c_write_blocking(i2c, dev_addr, &reg_addr, 1, true) != 1)
        return -__LINE__;
    if (i2c_read_blocking(i2c, dev_addr, reg_data, length, false) != (int)length)
        return -__LINE__;
    return 0;
}

static BME68X_INTF_RET_TYPE bme68x_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length, void *intf_ptr)
{
    uint8_t data[length + 1];
    data[0] = reg_addr;
    memcpy(&data[1], reg_data, length);

    i2c_inst_t *i2c = intf_ptr;
    if (i2c_write_blocking(i2c, dev_addr, data, length + 1, false) != (int)(length + 1))
        return -__LINE__;
    return 0;
}

static void bme68x_delay_us(uint32_t period, __unused void *intf_ptr)
{
    sleep_us(period);
}

int sensor_bme68x_init(i2c_inst_t *i2c)
{
    dev.read = &bme68x_i2c_read;
    dev.write = &bme68x_i2c_write;
    dev.delay_us = &bme68x_delay_us;
    dev.intf = BME68X_I2C_INTF;
    dev.intf_ptr = i2c;
    dev.amb_temp = 20; /* The ambient temperature in deg C is used for defining the heater temperature */

    if (bme68x_init(&dev) != BME68X_OK)
        return -__LINE__;

#if 0
    // Always read the current settings before writing, especially when all the configuration is not modified
    if (bme68x_get_conf(&conf, &dev) != BME68X_OK)
        return -__LINE__;
#endif

    conf.filter = BME68X_FILTER_OFF;
    conf.odr = BME68X_ODR_NONE;
    conf.os_hum = BME68X_OS_16X;
    conf.os_pres = BME68X_OS_1X;
    conf.os_temp = BME68X_OS_2X;
    if (bme68x_set_conf(&conf, &dev) != BME68X_OK)
        return -__LINE__;

    heatr_conf.enable = BME68X_ENABLE;
    heatr_conf.heatr_temp = 300;
    heatr_conf.heatr_dur = 100;
    if (bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, &dev) != BME68X_OK)
        return -__LINE__;

    return 0;
}

static int sensor_bme68x_update_temp(float temp)
{
    dev.amb_temp = round(temp);
    if (bme68x_set_heatr_conf(BME68X_FORCED_MODE, &heatr_conf, &dev) != BME68X_OK)
        return -__LINE__;
    return 0;
}

int sensor_bme68x_start_measurement(void)
{
    if (bme68x_set_op_mode(BME68X_FORCED_MODE, &dev) != BME68X_OK)
        return -__LINE__;

    absolute_time_t time = get_absolute_time();
    uint32_t meas_time = bme68x_get_meas_dur(BME68X_FORCED_MODE, &conf, &dev) + (heatr_conf.heatr_dur * 1000);

    time_wait = time + meas_time;
    return 0;
}

int sensor_bme68x_get_measurement(bme68x_result_t *result)
{
    sleep_until(time_wait);

#if 0
    struct bmp2_status status;
    do {
        if (bmp2_get_status(&status, &dev) != BMP2_OK)
            return -__LINE__;
    } while (status.measuring != BMP2_MEAS_DONE);
#endif

    struct bme68x_data data;
    uint8_t n_fields;
    if (bme68x_get_data(BME68X_FORCED_MODE, &data, &n_fields, &dev) != BME68X_OK)
        return -__LINE__;

    result->temperature = data.temperature;
    result->pressure = data.pressure;
    result->humidity = data.humidity;
    result->gas_resistance = data.gas_resistance;

    static int i = 0;
    if (i == 0) {
        i = 128;
        sensor_bme68x_update_temp(data.temperature);
    } else {
        i--;
    }
    return 0;
}
