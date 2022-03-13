#ifndef SENSOR_BME68X_H
#define SENSOR_BME68X_H

#include "hardware/i2c.h"

typedef struct
{
    /*! Temperature in degree celsius */
    float temperature;

    /*! Pressure in Pascal */
    float pressure;

    /*! Humidity in % relative humidity x1000 */
    float humidity;

    /*! Gas resistance in Ohms */
    float gas_resistance;
} bme68x_result_t;

int sensor_bme68x_init(i2c_inst_t *i2c);
int sensor_bme68x_start_measurement(void);
int sensor_bme68x_get_measurement(bme68x_result_t *result);

#endif // SENSOR_BME68X_H
