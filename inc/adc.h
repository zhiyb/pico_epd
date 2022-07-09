#pragma once

#include <stdint.h>

//#define NUM_ADC_IN  5
#define NUM_ADC_IN  0

typedef struct {
    uint32_t temp;
    uint32_t mvsys;
    uint32_t mv[NUM_ADC_IN];
} adc_results_t;

void sensor_adc_init(void);
adc_results_t sensor_adc_read(void);
