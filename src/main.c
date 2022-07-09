/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/unique_id.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "adc.h"
#include "sensor_bmp2.h"
#include "sensor_bme68x.h"
#include "sensor_bh1745.h"


#if 0
#define MEASUREMENT_INTERVAL_US     (1 * 1000 * 1000)
#define REPORT_INTERVAL_US          (10 * 1000 * 1000)
#else
#define MEASUREMENT_INTERVAL_US     (15 * 1000 * 1000)
#define REPORT_INTERVAL_US          (30 * 1000 * 1000)
#endif

#define ESP_POWER_SAVE              0
#define ESP_BOOT_US                 (1 * 1000 * 1000)

#define ESP_DMA_BUF_ADDR_BITS       12

#define I2C_SCAN                    0

#define RESULT_OK                   0


static i2c_inst_t * const i2c_sensors = i2c1;
static spi_inst_t * const spi_esp = spi0;
static const int spi_esp_tx_dma = 0;
static const int spi_esp_rx_dma = 1;
static const int pin_req = 1;
static const int pin_ack = 0;

static const unsigned int esp_pin = 22;

static char str_buf[8 * 1024];

static volatile struct {
    // SPI buffer for both TX and RX
    uint8_t buf[1 << ESP_DMA_BUF_ADDR_BITS];
} esp_dma_buf;

typedef struct {
    bool adc;
    bool bmp2;
    bool bme68x;
    bool bh1745;
} result_valid_t;

typedef struct {
    result_valid_t valid;
    adc_results_t adc;
    bmp2_result_t bmp2;
    bme68x_result_t bme68x;
    bh1745_result_t bh1745;
} result_t;

volatile struct {
    result_t data[256];
    uint8_t wr, rd;
} results;


static void init_i2c(void)
{
    const unsigned int gpio_sda = 18;
    gpio_set_pulls(gpio_sda, true, false);
    gpio_set_slew_rate(gpio_sda, GPIO_SLEW_RATE_SLOW);
    gpio_set_input_hysteresis_enabled(gpio_sda, true);
    gpio_set_function(gpio_sda, GPIO_FUNC_I2C);

    const unsigned int gpio_scl = 19;
    gpio_set_pulls(gpio_scl, true, false);
    gpio_set_slew_rate(gpio_scl, GPIO_SLEW_RATE_SLOW);
    gpio_set_input_hysteresis_enabled(gpio_scl, true);
    gpio_set_function(gpio_scl, GPIO_FUNC_I2C);

    i2c_init(i2c_sensors, 400 * 1000);
}

#if I2C_SCAN
static void i2c_scan(void)
{
    uint8_t __unused found[128] = {0};
    for (int addr = 0x03; addr <= 0x77; addr++) {
        uint8_t tmp;
        found[addr] = i2c_read_blocking(i2c_sensors, addr, &tmp, 1, false) != PICO_ERROR_GENERIC;
    }
    __breakpoint();
}
#endif

static void init_spi(void)
{
    // Actually maximum bitrate we can achieve in slave mode is 133MHz/12 = 11Mbps
    spi_init(spi_esp, clock_get_hz(clk_peri) / 2);
    hw_clear_bits(&spi_get_hw(spi_esp)->cr1, SPI_SSPCR1_SSE_BITS);
    spi_set_slave(spi_esp, true);
    spi_set_format(spi_esp, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    //hw_set_bits(&spi_get_hw(spi_esp)->cr1, SPI_SSPCR1_SSE_BITS);

    const unsigned int gpio_ncs = 5;
    //gpio_set_oeover(gpio_ncs, GPIO_OVERRIDE_LOW);
    gpio_set_pulls(gpio_ncs, true, false);
    //gpio_set_slew_rate(gpio_ncs, GPIO_SLEW_RATE_SLOW);
    gpio_set_input_hysteresis_enabled(gpio_ncs, true);
    gpio_set_function(gpio_ncs, GPIO_FUNC_SPI);
    // Set output disable on
    hw_set_bits(&padsbank0_hw->io[gpio_ncs], PADS_BANK0_GPIO0_OD_BITS);

    const unsigned int gpio_sck = 2;
    //gpio_set_oeover(gpio_sck, GPIO_OVERRIDE_LOW);
    gpio_set_pulls(gpio_sck, true, false);
    //gpio_set_slew_rate(gpio_sck, GPIO_SLEW_RATE_SLOW);
    gpio_set_input_hysteresis_enabled(gpio_sck, true);
    gpio_set_function(gpio_sck, GPIO_FUNC_SPI);
    // Set output disable on
    hw_set_bits(&padsbank0_hw->io[gpio_sck], PADS_BANK0_GPIO0_OD_BITS);

    const unsigned int gpio_tx  = 3;
    gpio_set_pulls(gpio_tx, false, false);
    gpio_set_slew_rate(gpio_tx, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(gpio_tx, GPIO_DRIVE_STRENGTH_12MA);
    //gpio_set_input_hysteresis_enabled(gpio_tx, true);
    gpio_set_function(gpio_tx, GPIO_FUNC_SPI);

    const unsigned int gpio_rx  = 4;
    //gpio_set_oeover(gpio_rx, GPIO_OVERRIDE_LOW);
    gpio_set_pulls(gpio_rx, true, false);
    //gpio_set_slew_rate(gpio_rx, GPIO_SLEW_RATE_SLOW);
    gpio_set_input_hysteresis_enabled(gpio_rx, true);
    gpio_set_function(gpio_rx, GPIO_FUNC_SPI);
    // Set output disable on
    hw_set_bits(&padsbank0_hw->io[gpio_rx], PADS_BANK0_GPIO0_OD_BITS);

    dma_channel_config tx_config = dma_channel_get_default_config(spi_esp_tx_dma);
    channel_config_set_read_increment(&tx_config, true);
    channel_config_set_write_increment(&tx_config, false);
    channel_config_set_dreq(&tx_config, spi_get_dreq(spi_esp, true));
    channel_config_set_transfer_data_size(&tx_config, DMA_SIZE_8);
    //channel_config_set_ring(&tx_config, false, ESP_DMA_BUF_ADDR_BITS);
    channel_config_set_enable(&tx_config, true);
    dma_channel_claim(spi_esp_tx_dma);
    dma_channel_configure(spi_esp_tx_dma, &tx_config, &spi_get_hw(spi_esp)->dr, esp_dma_buf.buf, sizeof(esp_dma_buf.buf), false);

    dma_channel_config rx_config = dma_channel_get_default_config(spi_esp_rx_dma);
    channel_config_set_read_increment(&rx_config, false);
    channel_config_set_write_increment(&rx_config, true);
    channel_config_set_dreq(&rx_config, spi_get_dreq(spi_esp, false));
    channel_config_set_transfer_data_size(&rx_config, DMA_SIZE_8);
    //channel_config_set_ring(&tx_config, true, ESP_DMA_BUF_ADDR_BITS);
    channel_config_set_enable(&rx_config, true);
    dma_channel_claim(spi_esp_rx_dma);
    dma_channel_configure(spi_esp_rx_dma, &rx_config, esp_dma_buf.buf, &spi_get_hw(spi_esp)->dr, sizeof(esp_dma_buf.buf), false);
}

static void init_gpio(void)
{
    const unsigned int led_pin = PICO_DEFAULT_LED_PIN;
    gpio_init(led_pin);
    gpio_set_dir(led_pin, GPIO_OUT);
    gpio_put(led_pin, true);

    gpio_init(esp_pin);
    gpio_set_dir(esp_pin, GPIO_OUT);
#if ESP_POWER_SAVE
    gpio_put(esp_pin, false);
#else
    gpio_put(esp_pin, true);
#endif

    gpio_init(pin_req);
    gpio_put(pin_req, 1);
    gpio_set_dir(pin_req, GPIO_OUT);
    gpio_set_pulls(pin_req, false, false);
    gpio_set_slew_rate(pin_req, GPIO_SLEW_RATE_SLOW);
    gpio_set_drive_strength(pin_req, GPIO_DRIVE_STRENGTH_4MA);
    //gpio_set_input_hysteresis_enabled(gpio_tx, true);

    gpio_init(pin_ack);
    gpio_set_dir(pin_ack, GPIO_IN);
    gpio_set_pulls(pin_ack, true, false);
    gpio_set_input_hysteresis_enabled(pin_ack, true);

    // ADC GPIOs as inputs
    for (unsigned int pin = 26; pin <= 29; pin++) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_set_pulls(pin, false, false);
    }
}

static void toggle_led(void)
{
    const unsigned int led_pin = PICO_DEFAULT_LED_PIN;
    static int led = 1;
    led = !led;
    gpio_put(led_pin, led);
}

static void esp_enable(bool en)
{
#if ESP_POWER_SAVE
    gpio_put(esp_pin, en);
#endif
}



static int send_data(const char *url, const char *data)
{
    // Prepare SPI data buffer
    const unsigned int urllen = strlen(url);
    const unsigned int datalen = strlen(data);
    uint8_t header[5] = {
        0xa5,           // POST
        urllen,
        urllen >> 8,
        datalen,
        datalen >> 8,
    };

    unsigned int ibuf = 0;
    memcpy((void *)&esp_dma_buf.buf[ibuf], header, 5);
    ibuf += 5;
    memcpy((void *)&esp_dma_buf.buf[ibuf], url, urllen);
    ibuf += urllen;
    memcpy((void *)&esp_dma_buf.buf[ibuf], data, datalen);
    ibuf += datalen;
    // Clear status code
    memset((void *)&esp_dma_buf.buf[ibuf], 0, 2);

    // Start SPI and send out data packet
    // Actually maximum bitrate we can achieve in slave mode is 133MHz/12 = 11Mbps
    spi_init(spi_esp, clock_get_hz(clk_peri) / 2);
    hw_clear_bits(&spi_get_hw(spi_esp)->cr1, SPI_SSPCR1_SSE_BITS);
    spi_set_slave(spi_esp, true);
    spi_set_format(spi_esp, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    hw_set_bits(&spi_get_hw(spi_esp)->cr1, SPI_SSPCR1_SSE_BITS);
    dma_channel_set_read_addr(spi_esp_tx_dma, esp_dma_buf.buf, true);
    dma_channel_set_write_addr(spi_esp_rx_dma, esp_dma_buf.buf, true);
    //dma_start_channel_mask((1 << spi_esp_tx_dma) | (1 << spi_esp_rx_dma));

    gpio_put(pin_req, 0);
    while (gpio_get(pin_ack) == 1);

    dma_channel_abort(spi_esp_rx_dma);
    dma_channel_abort(spi_esp_tx_dma);
    spi_deinit(spi_esp);

    gpio_put(pin_req, 1);
    while (gpio_get(pin_ack) == 0);

    uint16_t code = 0;
    memcpy(&code, (void *)&esp_dma_buf.buf[ibuf], 2);

    return code == 200 ? RESULT_OK : -code;
}

// Core 1 interrupt Handler
void core1_interrupt_handler()
{
    __breakpoint();
}

// Core 1 Main Code
void core1_entry(void)
{
    multicore_fifo_clear_irq();
    irq_set_enabled(SIO_IRQ_PROC1, true);


    static const char url_base[] = "https://zhiyb.me/logging/record.php?h=pico_";
    static char url[sizeof(url_base) + 2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES], *purl = url;
    memcpy(purl, url_base, sizeof(url_base));
    purl += sizeof(url_base) - 1;
    pico_get_unique_board_id_string(purl, sizeof(url) + (purl - url));


    result_valid_t prev_valid = {0};
    absolute_time_t time = get_absolute_time();
    for (;;) {
        uint8_t rd = results.rd;
        uint8_t wr = results.wr;
        if (rd == wr)
            continue;
        esp_enable(true);
#if ESP_POWER_SAVE
        sleep_us(ESP_BOOT_US);
#endif

        for (uint8_t i = /*rd*/ wr - 1; i != wr; i++) {
            const volatile result_t *pr = &results.data[i];
            result_valid_t valid = pr->valid;

            if ((valid.bmp2   && !prev_valid.bmp2  ) ||
                (valid.bme68x && !prev_valid.bme68x) ||
                (valid.adc    && !prev_valid.adc) ||
                (valid.bh1745 && !prev_valid.bh1745)) {

                bool comma = false;
                char *pstr = str_buf;
                static const char header[] = "{\"tables\":{\"sensors\":[";
                memcpy(pstr, header, sizeof(header));
                pstr += sizeof(header) - 1;

                if (valid.bmp2 && !prev_valid.bmp2) {
                    static const char null_bmp2[] = "{\"type\":\"temperature\",\"sensor\":\"bmp280\"},"
                                                    "{\"type\":\"pressure\",\"sensor\":\"bmp280\"}";
                    memcpy(pstr, null_bmp2, sizeof(null_bmp2));
                    pstr += sizeof(null_bmp2) - 1;
                    comma = true;
                }

                if (valid.bme68x && !prev_valid.bme68x) {
                    if (comma)
                        *pstr++ = ',';
                    static const char null_bme68x[] = "{\"type\":\"temperature\",\"sensor\":\"bme688\"},"
                                                      "{\"type\":\"pressure\",\"sensor\":\"bme688\"},"
                                                      "{\"type\":\"humidity\",\"sensor\":\"bme688\"},"
                                                      "{\"type\":\"gas_resistance\",\"sensor\":\"bme688\"}";
                    memcpy(pstr, null_bme68x, sizeof(null_bme68x));
                    pstr += sizeof(null_bme68x) - 1;
                    comma = true;
                }

                if (valid.adc && !prev_valid.adc) {
                    if (comma)
                        *pstr++ = ',';
#if NUM_ADC_IN == 0
                    static const char null[] = "{\"type\":\"temperature\",\"sensor\":\"rp2040\"},"
                                               "{\"type\":\"voltage\",\"sensor\":\"vsys\"}";
#else
                    static const char null[] = "{\"type\":\"temperature\",\"sensor\":\"rp2040\"},"
                                               "{\"type\":\"voltage\",\"sensor\":\"adc0\"},"
                                               "{\"type\":\"voltage\",\"sensor\":\"adc1\"},"
                                               "{\"type\":\"voltage\",\"sensor\":\"adc2\"},"
                                               "{\"type\":\"voltage\",\"sensor\":\"adc3\"},"
                                               "{\"type\":\"voltage\",\"sensor\":\"adc4\"}";
#endif
                    memcpy(pstr, null, sizeof(null));
                    pstr += sizeof(null) - 1;
                    comma = true;
                }

                if (valid.bh1745 && !prev_valid.bh1745) {
                    if (comma)
                        *pstr++ = ',';
                    static const char null[] = "{\"type\":\"luminance\",\"sensor\":\"bh1745_r\"},"
                                               "{\"type\":\"luminance\",\"sensor\":\"bh1745_g\"},"
                                               "{\"type\":\"luminance\",\"sensor\":\"bh1745_b\"},"
                                               "{\"type\":\"luminance\",\"sensor\":\"bh1745_c\"}";
                    memcpy(pstr, null, sizeof(null));
                    pstr += sizeof(null) - 1;
                    comma = true;
                }

                static const char footer[] = "]}}\r\n";
                memcpy(pstr, footer, sizeof(footer));
                pstr += sizeof(footer) - 1;

                if (send_data(url, str_buf) != RESULT_OK) {
                    prev_valid = (result_valid_t){0};
                    break;
                }
            }
            prev_valid = valid;

            {
                bool comma = false;
                char *pstr = str_buf;
                static const char header[] = "{\"tables\":{\"sensors\":[";
                memcpy(pstr, header, sizeof(header));
                pstr += sizeof(header) - 1;

                if (valid.bmp2) {
                    pstr += sprintf(pstr,
                                    "{\"type\":\"temperature\",\"sensor\":\"bmp280\",\"data\":%.3f},"
                                    "{\"type\":\"pressure\",\"sensor\":\"bmp280\",\"data\":%.3f}",
                                    pr->bmp2.temperature,
                                    pr->bmp2.pressure);
                    comma = true;
                }

                if (valid.bme68x) {
                    if (comma)
                        *pstr++ = ',';
                    pstr += sprintf(pstr,
                                    "{\"type\":\"temperature\",\"sensor\":\"bme688\",\"data\":%.3f},"
                                    "{\"type\":\"pressure\",\"sensor\":\"bme688\",\"data\":%.3f},"
                                    "{\"type\":\"humidity\",\"sensor\":\"bme688\",\"data\":%.3f},"
                                    "{\"type\":\"gas_resistance\",\"sensor\":\"bme688\",\"data\":%.3f}",
                                    pr->bme68x.temperature,
                                    pr->bme68x.pressure,
                                    pr->bme68x.humidity,
                                    pr->bme68x.gas_resistance);
                    comma = true;
                }

                if (valid.adc) {
                    if (comma)
                        *pstr++ = ',';
#if NUM_ADC_IN == 0
                    pstr += sprintf(pstr,
                                    "{\"type\":\"temperature\",\"sensor\":\"rp2040\",\"data\":%.3f},"
                                    "{\"type\":\"voltage\",\"sensor\":\"vsys\",\"data\":%.3f}",
                                    pr->adc.temp / 1000., pr->adc.mvsys / 1000.);
#else
                    pstr += sprintf(pstr,
                                    "{\"type\":\"temperature\",\"sensor\":\"rp2040\",\"data\":%.3f},"
                                    "{\"type\":\"voltage\",\"sensor\":\"adc0\",\"data\":%.3f},"
                                    "{\"type\":\"voltage\",\"sensor\":\"adc1\",\"data\":%.3f},"
                                    "{\"type\":\"voltage\",\"sensor\":\"adc2\",\"data\":%.3f},"
                                    "{\"type\":\"voltage\",\"sensor\":\"adc3\",\"data\":%.3f},"
                                    "{\"type\":\"voltage\",\"sensor\":\"adc4\",\"data\":%.3f}",
                                    pr->adc.temp / 1000.,
                                    pr->adc.mv[0] / 1000., pr->adc.mv[1] / 1000., pr->adc.mv[2] / 1000.,
                                    pr->adc.mv[3] / 1000., pr->adc.mv[4] / 1000.);
#endif
                    comma = true;
                }

                if (valid.bh1745) {
                    if (comma)
                        *pstr++ = ',';
                    pstr += sprintf(pstr,
                                    "{\"type\":\"luminance\",\"sensor\":\"bh1745_r\",\"data\":%.3f},"
                                    "{\"type\":\"luminance\",\"sensor\":\"bh1745_g\",\"data\":%.3f},"
                                    "{\"type\":\"luminance\",\"sensor\":\"bh1745_b\",\"data\":%.3f},"
                                    "{\"type\":\"luminance\",\"sensor\":\"bh1745_c\",\"data\":%.3f}",
                                    pr->bh1745.data[BH1745_Red] / 1000.,
                                    pr->bh1745.data[BH1745_Green] / 1000.,
                                    pr->bh1745.data[BH1745_Blue] / 1000.,
                                    pr->bh1745.data[BH1745_Clear] / 1000.);
                    comma = true;
                }

                static const char footer[] = "]}}\r\n";
                memcpy(pstr, footer, sizeof(footer));
                pstr += sizeof(footer) - 1;

                if (send_data(url, str_buf) != RESULT_OK) {
                    prev_valid = (result_valid_t){0};
                    break;
                }
            }
        }
        results.rd = wr;

        //send_data(url, str_buf);
        esp_enable(false);

        time = get_absolute_time();
        time += REPORT_INTERVAL_US;
        sleep_until(time);
    }
}


int main(void)
{
    init_gpio();
    init_i2c();
#if I2C_SCAN
    i2c_scan();
#endif
    sensor_bmp2_init(i2c_sensors);
    sensor_bme68x_init(i2c_sensors);
    sensor_bh1745_init(i2c_sensors);
    init_spi();
    sensor_adc_init();

    // Configure Core 1 Interrupt
    irq_set_exclusive_handler(SIO_IRQ_PROC1, core1_interrupt_handler);
    multicore_launch_core1(core1_entry);
    //multicore_fifo_push_blocking(0x3939);

    absolute_time_t time = get_absolute_time();
    for (;;) {
        result_valid_t valid = {0};
        valid.adc = true;
        valid.bmp2 = sensor_bmp2_start_measurement() == RESULT_OK;
        valid.bme68x = sensor_bme68x_start_measurement() == RESULT_OK;
        valid.bh1745 = sensor_bh1745_start_measurement() == RESULT_OK;

        volatile result_t *pv = &results.data[results.wr];
        pv->adc = sensor_adc_read();
        if (valid.bmp2)
            valid.bmp2 = sensor_bmp2_get_measurement((bmp2_result_t *)&pv->bmp2) == RESULT_OK;
        if (valid.bme68x)
            valid.bme68x = sensor_bme68x_get_measurement((bme68x_result_t *)&pv->bme68x) == RESULT_OK;
        if (valid.bh1745)
            valid.bh1745 = sensor_bh1745_get_measurement((bh1745_result_t *)&pv->bh1745) == RESULT_OK;
        pv->valid = valid;

        toggle_led();
        results.wr++;
        time += MEASUREMENT_INTERVAL_US;
        sleep_until(time);
        toggle_led();
    }
}
