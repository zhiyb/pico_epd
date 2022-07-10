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

#define UPDATE_INTERVAL_US          (60ull * 60ull * 1000ull * 1000ull)

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

#define URL_BASE                    "https://zhiyb.me/nas/api/disp.php"
#define SENSOR_URL_BASE             "https://zhiyb.me/logging/record.php?h=pico_"

#if EPD_TYPE == EPD_TYPE_4IN2_RWB
#define UUID_BASE                   "00000000-0000-0000-a0b7-6266088a6175"
#elif EPD_TYPE == EPD_TYPE_5IN65_FULL
#define UUID_BASE                   "00000000-0000-0000-9651-58a134fcdcc1"
#else
#error Unknown EPD type
#endif

enum {NicReqGet = 0x5a, NicReqPost = 0xa5};

static i2c_inst_t * const i2c_sensors = i2c1;
static spi_inst_t * const spi_esp = spi0;
static const int spi_esp_tx_dma = 0;
static const int spi_esp_rx_dma = 1;
static const int pin_req = 1;
static const int pin_ack = 0;
static const unsigned int gpio_esp_spi_ncs = 5;
static const unsigned int gpio_esp_spi_sck = 2;
static const unsigned int gpio_esp_spi_tx  = 3;
static const unsigned int gpio_esp_spi_rx  = 4;

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

    //gpio_set_oeover(gpio_ncs, GPIO_OVERRIDE_LOW);
    gpio_set_pulls(gpio_esp_spi_ncs, false, true);
    //gpio_set_slew_rate(gpio_ncs, GPIO_SLEW_RATE_SLOW);
    gpio_set_input_hysteresis_enabled(gpio_esp_spi_ncs, true);
    gpio_set_function(gpio_esp_spi_ncs, GPIO_FUNC_SPI);
    // Set output disable on
    hw_set_bits(&padsbank0_hw->io[gpio_esp_spi_ncs], PADS_BANK0_GPIO0_OD_BITS);

    //gpio_set_oeover(gpio_sck, GPIO_OVERRIDE_LOW);
    gpio_set_pulls(gpio_esp_spi_sck, false, true);
    //gpio_set_slew_rate(gpio_sck, GPIO_SLEW_RATE_SLOW);
    gpio_set_input_hysteresis_enabled(gpio_esp_spi_sck, true);
    gpio_set_function(gpio_esp_spi_sck, GPIO_FUNC_SPI);
    // Set output disable on
    hw_set_bits(&padsbank0_hw->io[gpio_esp_spi_sck], PADS_BANK0_GPIO0_OD_BITS);

    gpio_set_pulls(gpio_esp_spi_tx, false, false);
    gpio_set_slew_rate(gpio_esp_spi_tx, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(gpio_esp_spi_tx, GPIO_DRIVE_STRENGTH_12MA);
    //gpio_set_input_hysteresis_enabled(gpio_tx, true);
    gpio_set_function(gpio_esp_spi_tx, GPIO_FUNC_SPI);

    //gpio_set_oeover(gpio_rx, GPIO_OVERRIDE_LOW);
    gpio_set_pulls(gpio_esp_spi_rx, false, true);
    //gpio_set_slew_rate(gpio_rx, GPIO_SLEW_RATE_SLOW);
    gpio_set_input_hysteresis_enabled(gpio_esp_spi_rx, true);
    gpio_set_function(gpio_esp_spi_rx, GPIO_FUNC_SPI);
    // Set output disable on
    hw_set_bits(&padsbank0_hw->io[gpio_esp_spi_rx], PADS_BANK0_GPIO0_OD_BITS);

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

    // Default ESP and SPI IOs to pull-down for power saving
    gpio_init(esp_pin);
    gpio_set_pulls(esp_pin, false, false);
    gpio_set_dir(esp_pin, GPIO_OUT);
#if ESP_POWER_SAVE
    gpio_put(esp_pin, true);
#else
    gpio_put(esp_pin, false);
#endif

    gpio_init(pin_req);
    gpio_put(pin_req, 0);
    gpio_set_dir(pin_req, GPIO_OUT);
    gpio_set_pulls(pin_req, false, false);
    gpio_set_slew_rate(pin_req, GPIO_SLEW_RATE_SLOW);
    gpio_set_drive_strength(pin_req, GPIO_DRIVE_STRENGTH_4MA);
    //gpio_set_input_hysteresis_enabled(gpio_tx, true);

    gpio_init(pin_ack);
    gpio_set_dir(pin_ack, GPIO_IN);
    gpio_set_pulls(pin_ack, false, true);
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
    gpio_put(led_pin, en);
}

static void esp_enable(bool en)
{
#if ESP_POWER_SAVE
    if (en) {
        gpio_put(esp_pin, 0);

        sleep_us(ESP_BOOT_US);

    } else {
        gpio_put(esp_pin, 1);
    }
#endif
}

static const uint8_t *http_req(const char *url, unsigned int *resp,
                               const char *data, unsigned int datalen)
{
    const unsigned int headerlen = data ? 7 : 5;

    // Prepare SPI data buffer
    const unsigned int urllen = strlen(url);
    uint8_t header[] = {
        data ? NicReqPost : NicReqGet,
        urllen,
        urllen >> 8,
        datalen,
        datalen >> 8,
        0, 0,
    };

    unsigned int ibuf = 0;
    // Skip header for now
    ibuf += headerlen;
    memcpy((void *)&esp_dma_buf.buf[ibuf], url, urllen);
    ibuf += urllen;
    memcpy((void *)&esp_dma_buf.buf[ibuf], data, datalen);
    ibuf += datalen;
    // Clear status code and response size
    memset((void *)&esp_dma_buf.buf[ibuf], 0, 4);

    // Update resp buffer size, request header
    // response code (2), response size (2)
    unsigned int resplen = sizeof(esp_dma_buf.buf) - (ibuf + 2 + 2);
    memcpy(&header[headerlen - 2], &resplen, 2);
    memcpy((void *)&esp_dma_buf.buf[0], header, headerlen);

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

    gpio_put(pin_req, 1);
    while (gpio_get(pin_ack) == 1);

    gpio_put(pin_req, 0);
    dma_channel_abort(spi_esp_rx_dma);
    dma_channel_abort(spi_esp_tx_dma);
    spi_deinit(spi_esp);
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
    static const char url_base[] = URL_BASE "?get=";
    char url[256], *purl = url;
    memcpy(purl, url_base, sizeof(url_base));
    purl += sizeof(url_base) - 1;
    unsigned int len = strlen(uuid);
    memcpy(purl, uuid, len);
    purl += len;

    const unsigned int block = 1024 * 4;
    unsigned int ofs = 0;
    for (;;) {
        sprintf(purl, "&size=%u&ofs=%u", block, ofs);

        unsigned int resplen = 0;
        const uint8_t *resp = 0;
        for (int retry = 0; retry < 5; retry++) {
            resp = http_req(url, &resplen, 0, 0);
            if (resp != 0)
                break;
        }
        if (resp == 0)
            return 0;

        memcpy(&disp.raw[ofs], resp, resplen);
        ofs += resplen;
        if (resplen != block)
            break;
    }

    return &disp.raw[0];
}

void update_sensors(void)
{
    static const char url_base[] = SENSOR_URL_BASE;
    char url[256], *purl = url;
    memcpy(purl, url_base, sizeof(url_base));
    purl += sizeof(url_base) - 1;
    pico_get_unique_board_id_string(purl, sizeof(url) + (purl - url));

    adc_results_t adc = sensor_adc_read();

    char data[256];
    unsigned int datalen = snprintf(data, sizeof(data),
             "{\"tables\":{\"sensors\":["
             "{\"type\":\"temperature\",\"sensor\":\"rp2040\",\"data\":%g},"
             "{\"type\":\"voltage\",\"sensor\":\"vsys\",\"data\":%g}"
             "]}}\r\n", adc.temp / 1000., adc.mvsys / 1000.);

    unsigned int resplen = 0;
    const uint8_t *resp = 0;
    for (int retry = 0; retry < 5; retry++) {
        resp = http_req(url, &resplen, data, datalen);
        if (resp != 0)
            break;
    }
}

void update(const char *uuid)
{
    // Select EPD type
#if EPD_TYPE == EPD_TYPE_4IN2_RWB
    const epd_func_t epd_func = epd_func_4in2();
#elif EPD_TYPE == EPD_TYPE_5IN65_FULL
    const epd_func_t epd_func = epd_func_5in65();
#else
#error Unknown EPD type
    //epd_test_4in2();
    //epd_test_7in5();
    //epd_test_7in5_480p();
    //epd_test_5in65();
#endif

    // Start EPD update
    esp_enable(true);
    const uint8_t *pdata = get_disp_data(uuid);
    if (pdata) {
        led_en(true);
        epd_func.init();
        epd_func.update(pdata);
    }

    // Update sensor data
    update_sensors();

    // Network access complete
    esp_enable(false);

    // Wait EPD update complete
    if (pdata)
        epd_func.wait();
    led_en(false);
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

    const char *uuid = get_uuid();

    absolute_time_t time = get_absolute_time();
    for (;;) {
        update(uuid);
        time += UPDATE_INTERVAL_US;
        sleep_until(time);
    }
}
