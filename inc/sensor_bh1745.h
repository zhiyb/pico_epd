#pragma once

#include "hardware/i2c.h"

typedef enum {
    BH1745_Red, BH1745_Green, BH1745_Blue, BH1745_Clear,
    BH1745_NumColours
} bh1745_colours_t;

typedef struct {
    uint32_t data[BH1745_NumColours];
} bh1745_result_t;

int sensor_bh1745_init(i2c_inst_t *p_i2c);
int sensor_bh1745_start_measurement(void);
int sensor_bh1745_get_measurement(bh1745_result_t *result);
