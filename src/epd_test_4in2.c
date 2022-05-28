#include "epd_test.h"

#define EPD_WIDTH       400
#define EPD_HEIGHT      300
#define EPD_IMAGE_SIZE  (EPD_HEIGHT * ((EPD_WIDTH + 7) / 8))

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
    systick_delay(100);
    gpio_set_rst(0);
    systick_delay(2);
    gpio_set_rst(1);
    systick_delay(100);
}

static void epd_off()
{
    spi_master_enable(0);
    gpio_set_rst(0);
}

static void epd_busy()
{
    sleep_ms(100);
    while (gpio_get_busy())
        systick_delay(10);
}

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

static uint8_t epd_data_read()
{
    gpio_set_dc(1);
    uint8_t val = spi_master_receive();
    return val;
}

static void epd_read(int en)
{
    spi_receive_en(en);
}

static void epd_window(uint32_t Xstart, uint32_t Ystart, uint32_t Xend, uint32_t Yend)
{
    epd_cmd(CMD_SET_RAM_X_ADDRESS);
    epd_data((Xstart>>3) & 0xFF);
    epd_data((Xend>>3) & 0xFF);

    epd_cmd(CMD_SET_RAM_Y_ADDRESS);
    epd_data(Ystart & 0xFF);
    epd_data((Ystart >> 8) & 0xFF);
    epd_data(Yend & 0xFF);
    epd_data((Yend >> 8) & 0xFF);
}

static void epd_cursor(uint32_t Xstart, uint32_t Ystart)
{
    epd_cmd(CMD_SET_RAM_X_COUNTER);
    epd_data(Xstart & 0xFF);

    epd_cmd(CMD_SET_RAM_Y_COUNTER);
    epd_data(Ystart & 0xFF);
    epd_data((Ystart >> 8) & 0xFF);
}

static void epd_update(void)
{
    epd_cmd(CMD_MASTER_ACTIVATION);
    epd_busy();
}

static void epd_clear(void)
{
    static const uint32_t w = (EPD_WIDTH + 7) / 8, h = EPD_HEIGHT;

    epd_cmd(CMD_WRITE_RAM_BW);
    for (uint32_t j = 0; j < h; j++)
        for (uint32_t i = 0; i < w; i++)
            epd_data(0xFF);

    epd_cmd(CMD_WRITE_RAM_RED);
    for (uint32_t j = 0; j < h; j++)
        for (uint32_t i = 0; i < w; i++)
            epd_data(0x00);

    epd_update();
}

static void epd_display(const uint8_t *bw, const uint8_t *red)
{
    static const uint32_t w = (EPD_WIDTH + 7) / 8, h = EPD_HEIGHT;

    epd_cmd(CMD_WRITE_RAM_BW);
    for (uint32_t j = 0; j < h; j++)
        for (uint32_t i = 0; i < w; i++)
            epd_data(bw[i + j * w]);

    epd_cmd(CMD_WRITE_RAM_RED);
    for (uint32_t j = 0; j < h; j++)
        for (uint32_t i = 0; i < w; i++)
            epd_data(~red[i + j * w]);

    epd_update();
}

static void epd_init(void)
{
    epd_reset();

    epd_busy();
    epd_cmd(CMD_SW_RESET);
    epd_busy();

    epd_cmd(CMD_ANALOG_BLOCK_CONTROL);
    epd_data(0x54);
    epd_cmd(CMD_DIGITAL_BLOCK_CONTROL);
    epd_data(0x3B);
    epd_busy();

#if 0
    epd_cmd(CMD_OTP_READ);
    epd_read(1);
    uint8_t votp[10];
    for (unsigned int i = 0; i < sizeof(votp); i++)
        votp[i] = epd_data_read();
    spi_master_enable(0);
    epd_read(0);

    epd_cmd(CMD_USER_ID_READ);
    epd_read(1);
    uint8_t v[10];
    for (unsigned int i = 0; i < sizeof(v); i++)
        v[i] = epd_data_read();
    spi_master_enable(0);
    epd_read(0);

    epd_cmd(CMD_TEMP_READ);
    epd_read(1);
    uint8_t tv[2];
    for (unsigned int i = 0; i < sizeof(tv); i++)
        tv[i] = epd_data_read();
    spi_master_enable(0);
    epd_read(0);

#if 1
    __breakpoint();
#else
    printf(ESC_READ "%lu\tepd: GDEH042Z96 OTP: " ESC_DATA, systick_cnt());
    for (unsigned int i = 0; i < sizeof(votp); i++)
        printf("0x%02x, ", (int)votp[i]);
    printf("\n");

    printf(ESC_READ "%lu\tepd: GDEH042Z96 user ID: " ESC_DATA, systick_cnt());
    for (unsigned int i = 0; i < sizeof(v); i++)
        printf("0x%02x, ", (int)v[i]);
    printf("\n");

    printf(ESC_READ "%lu\tepd: GDEH042Z96 temp read: " ESC_DATA, systick_cnt());
    for (unsigned int i = 0; i < sizeof(tv); i++)
        printf("0x%02x, ", (int)tv[i]);
    printf("\n");

    flushCache();
    systick_delay(1000);
    goto start;
#endif
#endif

    epd_cmd(CMD_SOFT_START_CONTROL);
    epd_data(0x8e);
    epd_data(0x8c);
    epd_data(0x85);
    epd_data(0x3f);
    epd_cmd(CMD_ACVCOM_SETTING);
    epd_data(0x04);
    epd_data(0x63);
#if 1
    epd_cmd(0x2c);  // VCOM
    epd_data(0x50); // -2V
#endif

    epd_cmd(CMD_DRIVER_OUTPUT_CONTROL);
    epd_data(0x2B);		// Y low byte
    epd_data(0x01);		// Y high byte
    epd_data(0x00);

    epd_cmd(CMD_DATA_ENTRY_MODE_SETTING);
    epd_data(0x03);		// X-mode

    epd_cmd(CMD_BORDER_WAVEFORM_CONTROL);
    epd_data(0x01);

    // Use the internal temperature sensor
    epd_cmd(CMD_TEMP_SENSOR_CONTROL);
    epd_data(0x80);

#if 1
    // Display mode 1
    epd_cmd(CMD_DISPLAY_UPDATE_CONTROL2);
    epd_data(0xB1);
    epd_cmd(0x20);
    epd_busy();

    epd_cmd(CMD_DISPLAY_UPDATE_CONTROL2);
    epd_data(0xC7);
#else
    // Display mode 2
    epd_cmd(CMD_DISPLAY_UPDATE_CONTROL2);
    epd_data(0xB9);
    epd_cmd(0x20);
    epd_busy();

    epd_cmd(CMD_DISPLAY_UPDATE_CONTROL2);
    epd_data(0xCF);
#endif


#if 1
    static const uint8_t lut_full[76] =
    {
        0x66,0x88,0x66,0x21,0x54,0x40,0x04,
        0x66,0x10,0x66,0x21,0xa2,0x00,0x80,
        0x66,0x0a,0x66,0x21,0x60,0x2b,0x23,
        0x66,0x0a,0x66,0x21,0x60,0x2b,0x23,
        0x00,0x00,0x00,0x12,0x60,0x00,0x00,
        0x0f,0x01,0x0f,0x01,0x02,
        0x08,0x32,0x33,0x1c,0x00,
        0x02,0x04,0x01,0x03,0x13,
        0x01,0x14,0x01,0x14,0x04,
        0x02,0x04,0x0a,0x11,0x01,
        0x03,0x04,0x04,0x24,0x04,
        0x03,0x08,0x01,0x28,0x03,
        0x0f,0x32,0xaa,0x26,0x08,0x07,
    };

    uint32_t count;
    epd_cmd(0x32);
    for(count=0; count<70; count++) {
        epd_data(lut_full[count]);
    }
    epd_busy();

    epd_cmd(0x3C);
    epd_data(0x01);
#endif

#if 0
    static const uint8_t WF_PARTIAL_4IN2[76] =
    {
    0X00,0x40,0x00,0x00,0x00,0x00,0x00,	// L0 0-6 ABCD
    0X80,0x80,0x00,0x00,0x00,0x00,0x00,	// L1 0-6 ABCD
    0X40,0x40,0x00,0x00,0x00,0x00,0x00,	// L2 0-6 ABCD
    0X00,0x80,0x00,0x00,0x00,0x00,0x00,	// L3 0-6 ABCD
    0X00,0x00,0x00,0x00,0x00,0x00,0x00,	// VCOM 0-6 ABCD
    0x0A,0x00,0x00,0x00,0x02,//0A 0B 0C 0D R
    0x01,0x00,0x00,0x00,0x00,//1A 0B 0C 0D R
    0x00,0x00,0x00,0x00,0x00,//2A 0B 0C 0D R
    0x00,0x00,0x00,0x00,0x00,//3A 0B 0C 0D R
    0x00,0x00,0x00,0x00,0x00,//4A 0B 0C 0D R
    0x00,0x00,0x00,0x00,0x00,//5A 0B 0C 0D R
    0x00,0x00,0x00,0x00,0x00,//6A 0B 0C 0D R
    0x17,0x41,0x0, 0x32,0x2C,0x0A
    };

    uint32_t count;
    epd_cmd(0x32);
    for(count=0; count<70; count++) {
        epd_data(WF_PARTIAL_4IN2[count]);
    }
    epd_busy();
#endif

#if 0
    //epd_cmd(CMD_DISPLAY_UPDATE_CONTROL2);
    //epd_data(0xC7);

    epd_cmd(0x33);
    epd_read(1);
    uint8_t lut[76];
    for (unsigned int i = 0; i < sizeof(lut); i++)
        lut[i] = epd_data_read();
    spi_master_enable(0);
    epd_read(0);
    __breakpoint();
#endif




    epd_window(0, 0, EPD_WIDTH-1, EPD_HEIGHT-1);
    epd_cursor(0, 0);
    epd_busy();

    epd_clear();

#if 0
    epd_cmd(CMD_STATUS);
    epd_read(1);
    uint8_t status[2];
    for (unsigned int i = 0; i < sizeof(status); i++)
        status[i] = epd_data_read();
    spi_master_enable(0);
    epd_read(0);
    __breakpoint();
#endif

    //epd_off();

#if DEBUG >= DEBUG_PRINT
    printf(ESC_INIT "%lu\tepd: GDEH042Z96 init done\n", systick_cnt());
#endif
}

void epd_test_4in2(void)
{
    // GDEH042Z96-210119.pdf

    epd_init();
    printf(ESC_MSG "%lu\tepd: GDEH042Z96 test\n", systick_cnt());

    static const uint8_t img_bw[EPD_IMAGE_SIZE] = {
    #include "tp2_400x300_k.c"
    };
    static const uint8_t img_red[EPD_IMAGE_SIZE] = {
    #include "tp2_400x300_r.c"
    };

    epd_display(img_bw, img_red);

    epd_off();
}
