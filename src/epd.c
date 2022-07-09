#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "epd.h"

void init_epd(void)
{
    // Maximum bitrate we can use is 20MHz (133MHz/7)
    spi_init(spi_epd, 2 * MHZ);
    //hw_clear_bits(&spi_get_hw(spi_epd)->cr1, SPI_SSPCR1_SSE_BITS);
    //spi_set_slave(spi_epd, false);
    //spi_set_format(spi_epd, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    //hw_set_bits(&spi_get_hw(spi_epd)->cr1, SPI_SSPCR1_SSE_BITS);

#if 1
    gpio_init(gpio_epd_ncs);
    gpio_put(gpio_epd_ncs, 1);
    gpio_set_dir(gpio_epd_ncs, GPIO_OUT);
    gpio_set_pulls(gpio_epd_ncs, false, false);
    gpio_set_slew_rate(gpio_epd_ncs, GPIO_SLEW_RATE_SLOW);
    gpio_set_drive_strength(gpio_epd_ncs, GPIO_DRIVE_STRENGTH_4MA);
#else
    gpio_set_pulls(gpio_epd_ncs, false, false);
    gpio_set_slew_rate(gpio_epd_ncs, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(gpio_epd_ncs, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_function(gpio_epd_ncs, GPIO_FUNC_SPI);
#endif

    const unsigned int gpio_sck = 10;
    gpio_set_pulls(gpio_sck, false, false);
    gpio_set_slew_rate(gpio_sck, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(gpio_sck, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_function(gpio_sck, GPIO_FUNC_SPI);

    const unsigned int gpio_tx  = 11;
    gpio_set_pulls(gpio_tx, false, false);
    gpio_set_slew_rate(gpio_tx, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(gpio_tx, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_function(gpio_tx, GPIO_FUNC_SPI);

    const unsigned int gpio_rx  = 12;
    gpio_set_pulls(gpio_rx, true, false);
    gpio_set_input_hysteresis_enabled(gpio_rx, true);
    gpio_set_function(gpio_rx, GPIO_FUNC_SPI);
    // Set output disable on
    hw_set_bits(&padsbank0_hw->io[gpio_rx], PADS_BANK0_GPIO0_OD_BITS);

    gpio_init(gpio_epd_busy);
    gpio_set_dir(gpio_epd_busy, GPIO_IN);
    gpio_set_pulls(gpio_epd_busy, true, false);
    gpio_set_input_hysteresis_enabled(gpio_epd_busy, true);

    gpio_init(gpio_epd_dc);
    gpio_put(gpio_epd_dc, 1);
    gpio_set_dir(gpio_epd_dc, GPIO_OUT);
    gpio_set_pulls(gpio_epd_dc, false, false);
    gpio_set_slew_rate(gpio_epd_dc, GPIO_SLEW_RATE_SLOW);
    gpio_set_drive_strength(gpio_epd_dc, GPIO_DRIVE_STRENGTH_4MA);

    gpio_init(gpio_epd_rst);
    gpio_put(gpio_epd_rst, 1);
    gpio_set_dir(gpio_epd_rst, GPIO_OUT);
    gpio_set_pulls(gpio_epd_rst, false, false);
    gpio_set_slew_rate(gpio_epd_rst, GPIO_SLEW_RATE_SLOW);
    gpio_set_drive_strength(gpio_epd_rst, GPIO_DRIVE_STRENGTH_4MA);

#if 0
    dma_channel_config tx_config = dma_channel_get_default_config(spi_epd_tx_dma);
    channel_config_set_read_increment(&tx_config, true);
    channel_config_set_write_increment(&tx_config, false);
    channel_config_set_dreq(&tx_config, spi_get_dreq(spi_epd, true));
    channel_config_set_transfer_data_size(&tx_config, DMA_SIZE_8);
    //channel_config_set_ring(&tx_config, false, ESP_DMA_BUF_ADDR_BITS);
    channel_config_set_enable(&tx_config, true);
    dma_channel_claim(spi_epd_tx_dma);
    dma_channel_configure(spi_epd_tx_dma, &tx_config, &spi_get_hw(spi_epd)->dr, esp_dma_buf.buf, sizeof(esp_dma_buf.buf), false);

    dma_channel_config rx_config = dma_channel_get_default_config(spi_epd_rx_dma);
    channel_config_set_read_increment(&rx_config, false);
    channel_config_set_write_increment(&rx_config, true);
    channel_config_set_dreq(&rx_config, spi_get_dreq(spi_epd, false));
    channel_config_set_transfer_data_size(&rx_config, DMA_SIZE_8);
    //channel_config_set_ring(&tx_config, true, ESP_DMA_BUF_ADDR_BITS);
    channel_config_set_enable(&rx_config, true);
    dma_channel_claim(spi_epd_rx_dma);
    dma_channel_configure(spi_epd_rx_dma, &rx_config, esp_dma_buf.buf, &spi_get_hw(spi_epd)->dr, sizeof(esp_dma_buf.buf), false);
#endif
}
