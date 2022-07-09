#ifndef EPD_H
#define EPD_H

#include "hardware/spi.h"

static spi_inst_t * const spi_epd = spi1;

static const unsigned int gpio_epd_busy = 9;
static const unsigned int gpio_epd_ncs  = 13;
static const unsigned int gpio_epd_dc   = 14;
static const unsigned int gpio_epd_rst  = 15;

void init_epd(void);

#endif // EPD_H
