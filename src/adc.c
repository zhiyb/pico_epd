#include "hardware/adc.h"
#include "adc.h"

static const uint32_t vref = 3300;          // mV
static const uint32_t range = (1 << 12);    // 12-bit ADC
static const unsigned int avg = 8;

void sensor_adc_init(void)
{
    adc_init();
    //adc_set_clkdiv(4);
}

static inline uint32_t adc_to_volt(uint16_t adc)
{
    return vref * adc;
}

static inline uint32_t volt_to_mv(uint32_t volt)
{
    return (volt + range / 2) / range;
}

static inline uint32_t volt_to_temp(uint32_t volt)
{
    // 27 - (volt/range/1000 - 0.706) / 0.001721;
    return 27*1000 - ((int32_t)volt - 706*4096) / 7;
}

adc_results_t sensor_adc_read(void)
{
    adc_results_t res = {0};

    adc_set_temp_sensor_enabled(true);
    for (unsigned int a = 0; a < avg; a++) {
        // Select internal temperature sensor
        adc_select_input(4);
        uint16_t value = adc_read();
        //adc_set_temp_sensor_enabled(false);
        uint32_t volt = adc_to_volt(value);
        res.temp += volt_to_temp(volt);

        adc_select_input(3);
        value = adc_read();
        // Pico board ADC voltage divider
        res.mvsys += volt_to_mv(adc_to_volt(value)) * 3;

        for (int i = 0; i < NUM_ADC_IN; i++) {
            adc_select_input(i);
            value = adc_read();
            res.mv[i] += volt_to_mv(adc_to_volt(value));
        }

    }
    adc_set_temp_sensor_enabled(false);

    res.temp = (res.temp + (avg / 2)) / avg;
    res.mvsys = (res.mvsys + (avg / 2)) / avg;
    for (int i = 0; i < NUM_ADC_IN; i++)
        res.mv[i] = (res.mv[i] + (avg / 2)) / avg;

    return res;
}
