#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "epd.h"
#include "epd_test.h"

int DEV_Module_Init(void)
{
    return 0;
}

void systick_delay(uint32_t ms)
{
    sleep_ms(ms);
}

void spi_master_enable(bool val)
{
    gpio_put(EPD_CS_PIN, !val);
}

void gpio_set_rst(bool val)
{
    gpio_put(EPD_RST_PIN, val);
}

void gpio_set_dc(bool val)
{
    gpio_put(EPD_DC_PIN, val);
}

bool gpio_get_busy(void)
{
    return gpio_get(EPD_BUSY_PIN) != 0;
}

void spi_master_transmit(uint8_t val)
{
    spi_write_blocking(spi_epd, &val, 1);
}

void spi_receive_en(bool en)
{
    const unsigned int gpio_tx  = 11;
    if (en) {
        // Disable TX pin output
        hw_set_bits(&padsbank0_hw->io[gpio_tx], PADS_BANK0_GPIO0_OD_BITS);
    } else {
        // Reenable TX pin output
        hw_clear_bits(&padsbank0_hw->io[gpio_tx], PADS_BANK0_GPIO0_OD_BITS);
    }
}

uint8_t spi_master_receive(void)
{
    // Receive from RX pin
    uint8_t val = 0;
    spi_read_blocking(spi_epd, 0xff, &val, 1);
    return val;
}
