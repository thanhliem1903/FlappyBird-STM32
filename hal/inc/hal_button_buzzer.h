#ifndef HAL_BUTTON_BUZZER_H
#define HAL_BUTTON_BUZZER_H
#include "stm32f1xx_hal.h"
#include <stdint.h>
uint8_t Button_Read(void);
uint8_t Button_PauseRead(void);
void Buzzer_Set(uint8_t state);
#endif
