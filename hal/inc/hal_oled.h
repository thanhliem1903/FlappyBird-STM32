#ifndef HAL_OLED_H
#define HAL_OLED_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* OLED SSD1306 128 x 64 via I2C1. */
void OLED_Init(void);
void OLED_Clear(void);
void OLED_Update(void);
void OLED_DrawPixel(int16_t x, int16_t y, uint8_t color);
void OLED_FillRect(int16_t x, int16_t y,
                   int16_t width, int16_t height, uint8_t color);
void OLED_DrawText(int16_t x, int16_t y, const char *text);
void OLED_DrawNumber(int16_t x, int16_t y, uint16_t number);

#endif /* HAL_OLED_H */
