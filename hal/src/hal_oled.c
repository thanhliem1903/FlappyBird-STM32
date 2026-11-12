#include "hal_oled.h"
#include "i2c.h"

#define OLED_I2C_ADDRESS (0x3Cu << 1)
#define OLED_WIDTH  128
#define OLED_HEIGHT 64

static uint8_t framebuffer[OLED_WIDTH * OLED_HEIGHT / 8];

static void OLED_Command(uint8_t command)
{
    (void)HAL_I2C_Mem_Write(&hi2c1, OLED_I2C_ADDRESS, 0x00,
                            I2C_MEMADD_SIZE_8BIT, &command, 1, 100);
}

void OLED_Init(void)
{
    static const uint8_t commands[] = {
        0xAE,                   /* Display OFF */
        0xD5, 0x80,             /* Display clock */
        0xA8, 0x3F,             /* Multiplex ratio: 64 rows */
        0xD3, 0x00,             /* Display offset */
        0x40,                   /* Display start line */
        0x8D, 0x14,             /* Charge pump ON */
        0x20, 0x02,             /* Page addressing mode */
        0xA1,                   /* Segment remap */
        0xC8,                   /* Scan direction */
        0xDA, 0x12,             /* COM pins */
        0x81, 0xCF,             /* Contrast */
        0xD9, 0xF1,             /* Pre-charge */
        0xDB, 0x40,             /* VCOMH */
        0xA4,                   /* RAM content display */
        0xA6,                   /* Normal display */
        0xAF                    /* Display ON */
    };

    for (uint16_t i = 0; i < sizeof(commands); ++i) {
        OLED_Command(commands[i]);
    }

    OLED_Clear();
    OLED_Update();
}

void OLED_Clear(void)
{
    for (uint16_t i = 0; i < sizeof(framebuffer); ++i) {
        framebuffer[i] = 0;
    }
}

void OLED_Update(void)
{
    for (uint8_t page = 0; page < 8; ++page) {
        OLED_Command((uint8_t)(0xB0u + page));
        OLED_Command(0x00); /* Column start low nibble */
        OLED_Command(0x10); /* Column start high nibble */
        (void)HAL_I2C_Mem_Write(&hi2c1, OLED_I2C_ADDRESS, 0x40,
                                I2C_MEMADD_SIZE_8BIT,
                                &framebuffer[(uint16_t)page * OLED_WIDTH],
                                OLED_WIDTH, 100);
    }
}

void OLED_DrawPixel(int16_t x, int16_t y, uint8_t color)
{
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) {
        return;
    }

    uint16_t index = (uint16_t)x + (uint16_t)(y / 8) * OLED_WIDTH;
    uint8_t mask = (uint8_t)(1u << (y % 8));

    if (color) {
        framebuffer[index] |= mask;
    } else {
        framebuffer[index] &= (uint8_t)~mask;
    }
}

void OLED_FillRect(int16_t x, int16_t y,
                   int16_t width, int16_t height, uint8_t color)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    int32_t x_end = (int32_t)x + width;
    int32_t y_end = (int32_t)y + height;
    int16_t x_start = x < 0 ? 0 : x;
    int16_t y_start = y < 0 ? 0 : y;
    if (x_end > OLED_WIDTH) x_end = OLED_WIDTH;
    if (y_end > OLED_HEIGHT) y_end = OLED_HEIGHT;
    for (int16_t yy = y_start; yy < y_end; ++yy) {
        for (int16_t xx = x_start; xx < x_end; ++xx) {
            OLED_DrawPixel(xx, yy, color);
        }
    }
}

/* Minimal 3x5 font: 0-9, A-Z, and space (3 columns per glyph). */
static const uint8_t font3x5[37][3] = {
    {0x1F,0x11,0x1F}, {0x00,0x1F,0x00}, {0x1D,0x15,0x17},
    {0x15,0x15,0x1F}, {0x07,0x04,0x1F}, {0x17,0x15,0x1D},
    {0x1F,0x15,0x1D}, {0x01,0x01,0x1F}, {0x1F,0x15,0x1F},
    {0x17,0x15,0x1F}, {0x1E,0x05,0x1E}, {0x1F,0x15,0x0A},
    {0x0E,0x11,0x11}, {0x1F,0x11,0x0E}, {0x1F,0x15,0x11},
    {0x1F,0x05,0x01}, {0x0E,0x11,0x1D}, {0x1F,0x04,0x1F},
    {0x11,0x1F,0x11}, {0x08,0x10,0x0F}, {0x1F,0x04,0x1B},
    {0x1F,0x10,0x10}, {0x1F,0x02,0x1F}, {0x1F,0x01,0x1E},
    {0x0E,0x11,0x0E}, {0x1F,0x05,0x02}, {0x0E,0x19,0x1E},
    {0x1F,0x0D,0x12}, {0x12,0x15,0x09}, {0x01,0x1F,0x01},
    {0x0F,0x10,0x0F}, {0x07,0x18,0x07}, {0x1F,0x08,0x1F},
    {0x1B,0x04,0x1B}, {0x03,0x1C,0x03}, {0x19,0x15,0x13},
    {0x00,0x00,0x00}
};

static void OLED_Character(int16_t x, int16_t y, char ch)
{
    uint8_t index = 36; /* Space; also used for unsupported characters. */
    if (ch >= '0' && ch <= '9') {
        index = (uint8_t)(ch - '0');
    } else if (ch >= 'A' && ch <= 'Z') {
        index = (uint8_t)(ch - 'A' + 10);
    }
    for (uint8_t col = 0; col < 3; ++col) {
        for (uint8_t row = 0; row < 5; ++row) {
            if (font3x5[index][col] & (1u << row)) {
                OLED_DrawPixel((int16_t)(x + col), (int16_t)(y + row), 1);
            }
        }
    }
}

void OLED_DrawText(int16_t x, int16_t y, const char *text)
{
    if (text == 0) return;
    while (*text != '\0' && x < OLED_WIDTH - 3) {
        OLED_Character(x, y, *text++);
        x = (int16_t)(x + 4);
    }
}

void OLED_DrawNumber(int16_t x, int16_t y, uint16_t number)
{
    char digits[5];
    uint8_t count = 0;
    do {
        digits[count++] = (char)('0' + (number % 10u));
        number /= 10u;
    } while (number != 0u && count < sizeof(digits));

    while (count > 0) {
        OLED_Character(x, y, digits[--count]);
        x = (int16_t)(x + 4);
    }
}
