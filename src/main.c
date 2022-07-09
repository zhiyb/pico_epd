/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
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
#include "epd.h"
#include "epd_test.h"

#if 0
#define MEASUREMENT_INTERVAL_US     (1 * 1000 * 1000)
#define REPORT_INTERVAL_US          (10 * 1000 * 1000)
#else
#define MEASUREMENT_INTERVAL_US     (15 * 1000 * 1000)
#define REPORT_INTERVAL_US          (30 * 1000 * 1000)
#endif

#define ESP_POWER_SAVE              1
#define ESP_BOOT_US                 (1 * 1000 * 1000)

#define ESP_DMA_BUF_ADDR_BITS       15

#define I2C_SCAN                    0

#define RESULT_OK                   0

#define UUID_BASE                   "00000000-0000-0000-a0b7-6266088a6175"
#define URL_BASE                    "https://zhiyb.me/nas/api/disp.php"

enum {NicReqGet = 0x5a, NicReqPost = 0xa5};

static i2c_inst_t * const i2c_sensors = i2c1;
static spi_inst_t * const spi_esp = spi0;
static const int spi_esp_tx_dma = 0;
static const int spi_esp_rx_dma = 1;
static const int pin_req = 1;
static const int pin_ack = 0;

static const unsigned int esp_pin = 22;

static volatile struct {
    // SPI buffer for both TX and RX
    uint8_t buf[1 << ESP_DMA_BUF_ADDR_BITS];
} esp_dma_buf;

typedef struct {
    bool adc;
} result_valid_t;

typedef struct {
    result_valid_t valid;
    adc_results_t adc;
} result_t;

volatile struct {
    result_t data[256];
    uint8_t wr, rd;
} results;

typedef union {
    struct {
        uint8_t img[2][400*300/8];
    } rwb_400_300;
    struct {
        uint8_t img[2][640*384/8];
    } rwb_640_384;
    struct {
        uint8_t img[2][800*480/8];
    } rwb_800_480;
    struct {
        uint8_t img[600*448*3/8];
    } full_600_448;
    uint8_t raw[0];
} disp_data_t;
static disp_data_t disp;


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

#if 0
static void toggle_led(void)
{
    const unsigned int led_pin = PICO_DEFAULT_LED_PIN;
    static int led = 1;
    led = !led;
    gpio_put(led_pin, led);
}
#endif

static void led_en(bool en)
{
    const unsigned int led_pin = PICO_DEFAULT_LED_PIN;
    gpio_put(led_pin, !en);
}

static void esp_enable(bool en)
{
#if ESP_POWER_SAVE
    gpio_put(esp_pin, en);
    if (en)
        sleep_us(ESP_BOOT_US);
#endif
}

static const uint8_t *http_get(const char *url, unsigned int *resp)
{
    // Prepare SPI data buffer
    const unsigned int urllen = strlen(url);
    uint8_t header[] = {
        NicReqGet,
        urllen,
        urllen >> 8,
        0, 0,
    };

    unsigned int ibuf = 0;
    //memcpy((void *)&esp_dma_buf.buf[ibuf], header, sizeof(header));
    ibuf += 5;
    memcpy((void *)&esp_dma_buf.buf[ibuf], url, urllen);
    ibuf += urllen;
    // Clear status code and response size
    memset((void *)&esp_dma_buf.buf[ibuf], 0, 4);

    // Update resp buffer size, request header
    // response code (2), response size (2)
    unsigned int resplen = sizeof(esp_dma_buf.buf) - (ibuf + 2 + 2);
    memcpy(&header[3], &resplen, 2);
    memcpy((void *)&esp_dma_buf.buf[0], header, sizeof(header));

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
    ibuf += 2;

    if (code != 200)
        return 0;

    uint16_t respret = 0;
    memcpy(&respret, (void *)&esp_dma_buf.buf[ibuf], 2);
    ibuf += 2;

    if (respret < resplen)
        resplen = respret;
    *resp = respret;

    return (uint8_t *)&esp_dma_buf.buf[ibuf];
}


char *get_uuid(void)
{
    char id[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];
    pico_get_unique_board_id_string(id, sizeof(id));

    static char uuid[] = UUID_BASE;
    char *p = &id[0], *puuid = &uuid[0];
    while (*p != 0 && *puuid != 0) {
        if (*puuid == '-') {
            puuid++;
            continue;
        }
        *puuid = tolower(*p);
        puuid++;
        p++;
    }
    return uuid;
}

const uint8_t *get_disp_data(const char *uuid)
{
    esp_enable(true);

    static const char url_base[] = URL_BASE "?get=";
    char url[256], *purl = url;
    memcpy(purl, url_base, sizeof(url_base));
    purl += sizeof(url_base) - 1;
    unsigned int len = strlen(uuid);
    memcpy(purl, uuid, len);
    purl += len;

    const unsigned int block = 4096;
    unsigned int ofs = 0;
    for (;;) {
        sprintf(purl, "&size=%u&ofs=%u", block, ofs);
        unsigned int resplen = 0;
        const uint8_t *resp = http_get(url, &resplen);
        memcpy(&disp.raw[ofs], resp, resplen);
        ofs += resplen;
        if (resplen != block)
            break;
    }

    return &disp.raw[0];
}

int main(void)
{
    init_gpio();
    led_en(false);
#if I2C_SCAN
    init_i2c();
    i2c_scan();
#endif
    sensor_adc_init();

    init_spi();
    init_epd();

    char *uuid = get_uuid();

#if 1
    const uint8_t *pdata = get_disp_data(uuid); //&disp.rwb_400_300.img[0][0];
    led_en(true);
    epd_test_4in2(pdata);
    led_en(false);
#endif

    for (;;)
        sleep_until(-1);

#if 0
    __breakpoint();
    for (;;) {
        //epd_test_4in2();
        //epd_test_7in5();
        //epd_test_7in5_480p();
        //epd_test_5in65();

        toggle_led();
        __breakpoint();
        sleep_ms(1000);
    }
#endif
}
