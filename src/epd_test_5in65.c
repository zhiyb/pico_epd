#include "epd_test.h"

#define EPD_WIDTH       600
#define EPD_HEIGHT      448
#define EPD_IMAGE_SIZE  (EPD_HEIGHT * ((EPD_WIDTH + 1) / 2))

#define CMD_DRIVER_OUTPUT_CONTROL   0x01
#define CMD_GATE_VOLTAGE_CONTROL    0x03
#define CMD_SOURCE_VOLTAGE_CONTROL  0x04
#define CMD_SOFT_START_CONTROL      0x0c
#define CMD_DATA_ENTRY_MODE_SETTING 0x11
#define CMD_SW_RESET                0x12
#define CMD_TEMP_SENSOR_CONTROL     0x18
#define CMD_TEMP_READ               0x1b
#define CMD_MASTER_ACTIVATION       0x20
#define CMD_DISPLAY_UPDATE_CONTROL1 0x21
#define CMD_DISPLAY_UPDATE_CONTROL2 0x22
#define CMD_WRITE_RAM_BW            0x24
#define CMD_WRITE_RAM_RED           0x26
#define CMD_ACVCOM_SETTING          0x2b
#define CMD_OTP_READ                0x2d
#define CMD_USER_ID_READ            0x2e
#define CMD_STATUS                  0x2f
#define CMD_BORDER_WAVEFORM_CONTROL 0x3c
#define CMD_SET_RAM_X_ADDRESS       0x44
#define CMD_SET_RAM_Y_ADDRESS       0x45
#define CMD_SET_RAM_X_COUNTER       0x4e
#define CMD_SET_RAM_Y_COUNTER       0x4f
#define CMD_ANALOG_BLOCK_CONTROL    0x74
#define CMD_DIGITAL_BLOCK_CONTROL   0x7e

#define DEBUG_PRINT	5

static void epd_reset()
{
    spi_master_enable(0);
    gpio_set_rst(1);
    sleep_ms(200);
    gpio_set_rst(0);
    sleep_ms(2);
    gpio_set_rst(1);
    sleep_ms(200);
}

static void epd_off()
{
    spi_master_enable(0);
    gpio_set_rst(0);
}

static void epd_busy()
{
    sleep_ms(100);
    while (!gpio_get_busy())
        systick_delay(10);
}

#if 0
static void epd_cmd(uint8_t cmd)
{
    spi_master_enable(0);
    spi_master_enable(1);
    gpio_set_dc(0);
    spi_master_transmit(cmd);
}

static void epd_data(uint8_t v)
{
    gpio_set_dc(1);
    spi_master_transmit(v);
}
#else
static void epd_cmd(uint8_t cmd)
{
    gpio_set_dc(0);
    spi_master_enable(1);
    spi_master_transmit(cmd);
    spi_master_enable(0);
}

static void epd_data(uint8_t v)
{
    gpio_set_dc(1);
    spi_master_enable(1);
    spi_master_transmit(v);
    spi_master_enable(0);
}
#endif

static inline uint8_t epd_data_read()
{
    gpio_set_dc(1);
    spi_master_enable(1);
    uint8_t val = spi_master_receive();
    spi_master_enable(0);
    return val;
}

static inline void epd_read(int en)
{
    spi_receive_en(en);
}

static void epd_update(void)
{
    epd_cmd(0x04);//0x04
    epd_busy();

    epd_cmd(0x12);//0x12
    epd_busy();
}

static void epd_clear(void)
{
    uint8_t color = 0x01;   // White

    epd_cmd(0x10);
    for (uint32_t i = 0; i < EPD_IMAGE_SIZE; i++)
        epd_data((color<<4)|color);

    epd_update();

    //sleep_ms(30 * 1000);
}

static void epd_display(const uint8_t *img)
{
    epd_cmd(0x10);
    for (uint32_t i = 0; i < EPD_IMAGE_SIZE; i++)
        epd_data(img[i]);

    epd_update();
}

static void epd_init(void)
{
    epd_reset();
    epd_busy();

    epd_cmd(0x00);
    epd_data(0xEF);
    epd_data(0x08);

    epd_cmd(0x01);
    epd_data(0x37);
    epd_data(0x00);
    epd_data(0x23);
    epd_data(0x23);

    epd_cmd(0x03);
    epd_data(0x00);

    epd_cmd(0x06);
    epd_data(0xC7);
    epd_data(0xC7);
    epd_data(0x1D);

    epd_cmd(0x30);
    epd_data(0x39);

    epd_cmd(0x41);
    epd_data(0x00);

    epd_cmd(0x50);
    epd_data(0x37);

    epd_cmd(0x60);
    epd_data(0x22);

    epd_cmd(0x61);
    epd_data(0x02);
    epd_data(0x58);
    epd_data(0x01);
    epd_data(0xC0);

    epd_cmd(0xE3);
    epd_data(0xAA);

    sleep_ms(100);

    epd_cmd(0x50);
    epd_data(0x37);

    epd_cmd(0x61);//Set Resolution setting
    epd_data(0x02);
    epd_data(0x58);
    epd_data(0x01);
    epd_data(0xC0);

    epd_busy();

    //epd_clear();
}

void epd_test_5in65(void)
{
    // GDEP0565D90.pdf

    epd_init();
    //printf(ESC_MSG "%lu\tepd: GDEH042Z96 test\n", systick_cnt());

    static const uint8_t img_f[EPD_IMAGE_SIZE] = {
    //#include "tp2_600x448_no_f.c"
    //#include "43010519_p0_f.c"
    //#include "43120444_p0_f.c"
    //#include "43190512_p0_f.c"
    //#include "43885635_p0_f.c"
    //#include "45454055_p0_f.c"
    //#include "47046948_p0_f.c"
    //#include "52283869_p0_f.c"
    //#include "55294320_p0_f.c"
    //#include "58733226_p0_f.c"
    //#include "59581340_p0_f.c"
    //#include "60867404_p0_f.c"
    #include "63402383_p0_f.c"
    //#include "65290423_p0_f.c"
    };

    epd_display(img_f);

    epd_off();
}
