#ifndef EPD_TEST_H
#define EPD_TEST_H

#include <stdbool.h>
#include <stdint.h>
#include "epd.h"

#define printf(...)
#define Debug(...)
#define flushCache(...)
#define dbgbkpt()   __breakpoint()


void epd_test_4in2(const uint8_t *pimg);
void epd_test_7in5(void);
void epd_test_7in5_480p(void);
void epd_test_5in65(void);


// HAL compatibility

void systick_delay(uint32_t ms);
void spi_master_enable(bool val);
void gpio_set_rst(bool val);
void gpio_set_dc(bool val);
bool gpio_get_busy(void);
void spi_receive_en(bool en);
void spi_master_transmit(uint8_t val);
uint8_t spi_master_receive(void);

#define EPD_RST_PIN     gpio_epd_rst
#define EPD_DC_PIN      gpio_epd_dc
#define EPD_CS_PIN      gpio_epd_ncs
#define EPD_BUSY_PIN    gpio_epd_busy

#endif // EPD_TEST_H
