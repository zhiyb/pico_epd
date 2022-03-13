#ifndef BMP280_H
#define BMP280_H

#include "hardware/i2c.h"

typedef struct
{
    /*! Compensated pressure */
    double pressure;

    /*! Compensated temperature */
    double temperature;
} bmp2_result_t;

int sensor_bmp2_init(i2c_inst_t *i2c);
int sensor_bmp2_start_measurement(void);
int sensor_bmp2_get_measurement(bmp2_result_t *result);

#endif // BMP280_H
