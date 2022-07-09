#include <string.h>
#include "hardware/i2c.h"
#include "sensor_bh1745.h"

#define BH1745_REG_SYSTEM_CONTROL 0x40
#define BH1745_REG_MODE_CONTROL1 0x41
#define BH1745_REG_MODE_CONTROL2 0x42
#define BH1745_REG_MODE_CONTROL3 0x44
#define BH1745_REG_COLOUR_DATA 0x50
#define BH1745_REG_DINT_DATA 0x58
#define BH1745_REG_INTERRUPT 0x60
#define BH1745_REG_PERSISTENCE 0x61
#define BH1745_REG_THRESHOLD_LOW 0x64
#define BH1745_REG_THRESHOLD_HIGH 0x62
#define BH1745_REG_MANUFACTURER 0x92

#define BH1745_CHIP_ID 0b001011
#define BH1745_MANUFACTURER 0xe0

static const int dev_addr = 0x38;   // Or 0x39

static i2c_inst_t *i2c = 0;

static int i2c_read_nbytes(i2c_inst_t *i2c, uint8_t reg_addr, uint8_t *reg_data, uint32_t length)
{
    if (i2c_write_blocking(i2c, dev_addr, &reg_addr, 1, true) != 1)
        return -__LINE__;
    if (i2c_read_blocking(i2c, dev_addr, reg_data, length, false) != (int)length)
        return -__LINE__;
    return 0;
}

static inline int i2c_read(i2c_inst_t *i2c, uint8_t reg_addr, uint8_t *reg_data)
{
    return i2c_read_nbytes(i2c, reg_addr, reg_data, 1);
}

static int i2c_write_nbytes(i2c_inst_t *i2c, uint8_t reg_addr, const uint8_t *reg_data, uint32_t length)
{
    uint8_t data[length + 1];
    data[0] = reg_addr;
    memcpy(&data[1], reg_data, length);

    if (i2c_write_blocking(i2c, dev_addr, data, length + 1, false) != (int)(length + 1))
        return -__LINE__;
    return 0;
}

static inline int i2c_write(i2c_inst_t *i2c, uint8_t reg_addr, const uint8_t reg_data)
{
    return i2c_write_nbytes(i2c, reg_addr, &reg_data, 1);
}

static int set_measurement_time_ms(i2c_inst_t *i2c, uint16_t value)
{
    uint8_t reg = 0;
    switch(value) {
        case 160:
            reg = 0b000;
            break;
        case 320:
            reg = 0b001;
            break;
        case 640:
            reg = 0b010;
            break;
        case 1280:
            reg = 0b011;
            break;
        case 2560:
            reg = 0b100;
            break;
        case 5120:
            reg = 0b101;
            break;
    }
    if (i2c_write(i2c, BH1745_REG_MODE_CONTROL1, reg))
        return -__LINE__;
    return 0;
}

static int set_threshold_high(i2c_inst_t *i2c, uint16_t value)
{
    if (i2c_write_nbytes(i2c, BH1745_REG_THRESHOLD_HIGH, (uint8_t *)&value, 2))
        return -__LINE__;
    return 0;
}

static int set_threshold_low(i2c_inst_t *i2c, uint16_t value)
{
    if (i2c_write_nbytes(i2c, BH1745_REG_THRESHOLD_LOW, (uint8_t *)&value, 2))
        return -__LINE__;
    return 0;
}

int sensor_bh1745_init(i2c_inst_t *p_i2c)
{
    i2c = p_i2c;

    // Reset
    if (i2c_write(i2c, BH1745_REG_SYSTEM_CONTROL, 1 << 7))
        return -__LINE__;
    for (;;) {
        uint8_t v;
        sleep_ms(10);
        if (i2c_read(i2c, BH1745_REG_SYSTEM_CONTROL, &v))
            return -__LINE__;
        if ((v & (1 << 7)) == 0)
            break;
    }

    // Clear INT reset bit
    if (i2c_write(i2c, BH1745_REG_SYSTEM_CONTROL, 0))
        return -__LINE__;
    if (set_measurement_time_ms(i2c, 640))
        return -__LINE__;
    // Enable RGBC
    if (i2c_write(i2c, BH1745_REG_MODE_CONTROL2, 1 << 4))
        return -__LINE__;
    // Turn on sensor
    if (i2c_write(i2c, BH1745_REG_MODE_CONTROL3, 0x02))
        return -__LINE__;
    // Set threshold so int will always fire
    if (set_threshold_high(i2c, 0x0000))
        return -__LINE__;
    // this lets us turn on the LEDs with the int pin
    if (set_threshold_low(i2c, 0xFFFF))
        return -__LINE__;
    // Enable interrupt latch
    if (i2c_write(i2c, BH1745_REG_INTERRUPT, 0))
        return -__LINE__;

    sleep_ms(320);

    return 0;
}

int sensor_bh1745_start_measurement(void)
{
    return 0;
}

int sensor_bh1745_get_measurement(bh1745_result_t *result)
{
    for (;;) {
        uint8_t v;
        if (i2c_read(i2c, BH1745_REG_MODE_CONTROL2, &v))
            return -__LINE__;
        if ((v & (1 << 7)) != 0)
            break;
        sleep_ms(1);
    }

    struct  {
        uint16_t r;
        uint16_t g;
        uint16_t b;
        uint16_t c;
    } data;
    if (i2c_read_nbytes(i2c, BH1745_REG_COLOUR_DATA, (uint8_t *)&data, 8))
        return -__LINE__;

    result->data[BH1745_Red]   = data.r * 1389;
    result->data[BH1745_Green] = data.g * 1000;
    result->data[BH1745_Blue]  = data.b * 1768;
    result->data[BH1745_Clear] = data.c * 22805;
    return 0;
}
