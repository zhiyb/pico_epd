#include "epd_test.h"

#define EPD_WIDTH       800
#define EPD_HEIGHT      480
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
    epd_cmd(0x12); // DISPLAY_REFRESH
    epd_busy();
}

static void epd_clear(void)
{
    static const uint32_t w = (EPD_WIDTH + 7) / 8, h = EPD_HEIGHT;
    uint32_t area = w * h;

    epd_cmd(0x10);
    for (uint32_t i = 0; i < area; i++)
        epd_data(0xff);

    epd_cmd(0x13);
    for (uint32_t i = 0; i < area; i++)
        epd_data(0x00);

    epd_update();

    //sleep_ms(30 * 1000);
}

static void epd_display(const uint8_t *bw, const uint8_t *red)
{
    static const uint32_t w = (EPD_WIDTH + 7) / 8, h = EPD_HEIGHT;
    uint32_t area = w * h;

#if 1
    (void)bw;
    (void)red;
    epd_cmd(0x10);
    for (uint32_t i = 0; i < area; i++)
        epd_data(0x00);

    epd_cmd(0x13);
    for (uint32_t i = 0; i < area; i++)
        epd_data(0x00);
#else
    epd_cmd(0x10);
    for (uint32_t i = 0; i < area; i++)
        epd_data(bw[i]);

    epd_cmd(0x13);
    for (uint32_t i = 0; i < area; i++)
        epd_data(red[i]);
#endif

    epd_update();

    //sleep_ms(30 * 1000);
}

static void epd_init(void)
{
    epd_reset();
    epd_busy();

    epd_cmd(0x00);			//PANNEL SETTING
    epd_data(0x0e);   // reset
    epd_busy();

#if 0
    uint8_t rev_data[7];
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

#if 1
    epd_cmd(0x00);			//PANNEL SETTING
    epd_data(0x2F);   //KW-3f   KWR-2F	BWROTP 0f	BWOTP 1f

    epd_cmd(0x01);			//POWER SETTING
    epd_data(0x07);
    epd_data(0x17);    //VGH=20V,VGL=-20V
    epd_data(0x3f);		//VDH=15V
    epd_data(0x3f);		//VDL=-15V
    epd_data(0x03);		//VDHR=3V

    epd_cmd(0x61);        	//tres
    epd_data(0x03);		//source 800
    epd_data(0x20);
    epd_data(0x01);		//gate 480
    epd_data(0xE0);

    epd_cmd(0x65);  // Resolution setting
    epd_data(0x00);
    epd_data(0x00);//800*480
    epd_data(0x00);
    epd_data(0x00);

    epd_cmd(0X15);
    epd_data(0x00);

#if 0
    epd_cmd(0x81);  // VCOM
    epd_data(0);
#endif

    epd_cmd(0x82);  // VCOM_DC
    epd_data(0b0100110);

    epd_cmd(0X50);			//VCOM AND DATA INTERVAL SETTING
    epd_data(0x21);
    epd_data(0x0f);

#if 0
    epd_cmd(0x52);
    epd_data(0x00);
#endif

    epd_cmd(0X60);			//TCON SETTING
    epd_data(0x22);
#endif

#if 1
    // VCOM LUT
    static const uint8_t vcom_lut[] = {
        0x1b, 2, 4, 8, 16, 128,
        0x1b, 2, 4, 8, 16, 128,
        0x1b, 2, 4, 8, 16, 128,
        0x1b, 2, 4, 8, 16, 128,
        0x1b, 2, 4, 8, 16, 128,
        0x1b, 2, 4, 8, 16, 128,
        0x1b, 2, 4, 8, 16, 128,
        0x1b, 2, 4, 8, 16, 128,
        0x1b, 2, 4, 8, 16, 128,
        0x1b, 2, 4, 8, 16, 128,
    };
    epd_cmd(0x20);
    for (uint32_t i = 0; i < sizeof(vcom_lut); i++)
        epd_data(vcom_lut[i]);
#endif

#if 0
    epd_cmd(0x2a);  // LUT option
    epd_data(0xc0);
    epd_data(0xff);
#endif

#if 0
    epd_cmd(0x50);  // VCOM and data interval
    epd_data(0x81);
    epd_data(0x00);
#endif

#if 0
    epd_cmd(0x60);  // TCON
    epd_data(0xff);
#endif

    epd_cmd(0x82);  // VCOM_DC
    epd_data(0b0100110);

    epd_cmd(0x04); //POWER ON
    epd_busy();

    epd_clear();
}

void epd_test_7in5_480p(void)
{
    // 7.5inch-e-paper-b-v3-specification.pdf

    epd_init();
    //printf(ESC_MSG "%lu\tepd: GDEH042Z96 test\n", systick_cnt());

    static const uint8_t img_bw[EPD_IMAGE_SIZE] = {
    #include "tp2_800x480_k.c"
    };
    static const uint8_t img_red[EPD_IMAGE_SIZE] = {
    #include "tp2_800x480_r.c"
    };

    epd_display(img_bw, img_red);

    epd_off();
}
