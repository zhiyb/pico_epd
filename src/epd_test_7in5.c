#include "epd_test.h"

#define EPD_WIDTH       640
#define EPD_HEIGHT      384
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
    spi_master_enable(1);
    uint8_t val = spi_master_receive();
    spi_master_enable(0);
    return val;
}

static void epd_read(int en)
{
    spi_receive_en(en);
}

static void epd_update(void)
{
    epd_cmd(0x12); // DISPLAY_REFRESH
    epd_busy();
}

static void epd_clear(void)
{
    static const uint32_t w = EPD_WIDTH, h = EPD_HEIGHT;
    uint32_t area = w * h;

    epd_cmd(0x10);

    for (uint32_t i = 0; i < area / 2; i++)
        epd_data(0x33);

    epd_update();
}

static void epd_display(const uint8_t *bw, const uint8_t *red)
{
    static const uint32_t w = EPD_WIDTH, h = EPD_HEIGHT;
    uint32_t area = w * h;

    epd_cmd(0x10);

    for (uint32_t i = 0; i < area / 2; i++) {
        uint8_t data = 0, val = 0;
        uint32_t start = i * 2;
        uint8_t c_bw = bw[start / 8] << (start % 8);
        uint8_t c_red = red[start / 8] << (start % 8);
        if (!(c_red & 0x80))
            val = 0b100;    // Red
        else if (c_bw & 0x80)
            val = 0b011;    // White
        else
            val = 0b000;    // Black
        data = val << 4;

        start += 1;
        c_bw = bw[start / 8] << (start % 8);
        c_red = red[start / 8] << (start % 8);
        if (!(c_red & 0x80))
            val = 0b100;    // Red
        else if (c_bw & 0x80)
            val = 0b011;    // White
        else
            val = 0b000;    // Black
        data |= val;

        epd_data(data);
    }

    epd_update();
}

static void epd_init(void)
{
    epd_reset();
    epd_busy();

#if 0
    uint8_t rev_data[3];
    epd_cmd(0x70);
    epd_read(1);
    for (unsigned int i = 0; i < 7; i++)
        rev_data[i] = epd_data_read();
    epd_read(0);

    uint8_t otp_data[0x1800];
    epd_cmd(0xa2);
    epd_read(1);
    for (unsigned int i = 0; i < sizeof(otp_data); i++)
        otp_data[i] = epd_data_read();
    epd_read(0);

    __breakpoint();
#endif

    epd_cmd(0x01); // POWER_SETTING
    epd_data(0x37);
    epd_data(0x00);

    epd_cmd(0x00); // PANEL_SETTING
    epd_data(0xCF);
    epd_data(0x08);

    epd_cmd(0x30); // PLL_CONTROL
    epd_data(0x3A); // PLL:  0-15:0x3C, 15+:0x3A

    epd_cmd(0x82); // VCM_DC_SETTING
    epd_data(0x28); //all temperature  range

    epd_cmd(0x06); // BOOSTER_SOFT_START
    epd_data (0xc7);
    epd_data (0xcc);
    epd_data (0x15);

    epd_cmd(0x50); // VCOM AND DATA INTERVAL SETTING
    epd_data(0x77);

    epd_cmd(0x60); // TCON_SETTING
    epd_data(0x22);

    epd_cmd(0x65); // FLASH CONTROL
    epd_data(0x00);

    epd_cmd(0x61);  // TCON_RESOLUTION
    epd_data(EPD_WIDTH >> 8); // source 640
    epd_data(EPD_WIDTH & 0xff);
    epd_data(EPD_HEIGHT >> 8); // gate 384
    epd_data(EPD_HEIGHT & 0xff);

    epd_cmd(0xe5); // FLASH MODE
    epd_data(0x03);

    epd_cmd(0x04); // POWER ON
    epd_busy();

    epd_clear();
}

void epd_test_7in5(void)
{
    // 7.5inch_e-paper-b-Specification.pdf

    epd_init();
    //printf(ESC_MSG "%lu\tepd: GDEH042Z96 test\n", systick_cnt());

    static const uint8_t img_bw[EPD_IMAGE_SIZE] = {
    #include "tp2_640x384_k.c"
    };
    static const uint8_t img_red[EPD_IMAGE_SIZE] = {
    #include "tp2_640x384_r.c"
    };

    epd_display(img_bw, img_red);

    epd_off();
}
