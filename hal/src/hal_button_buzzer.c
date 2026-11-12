#include "hal_button_buzzer.h"
#include "gpio.h"
#include "tim.h"

/* TIM1_CH1 on PA8. TIM1 runs at 72 MHz in this Blue Pill project.
 * 72 MHz / (71 + 1) = 1 MHz timer clock.
 * This is a PWM waveform for a *low-current passive piezo buzzer*.
 * An passsive 3 V buzzer or an unverified high-current buzzer must NOT
 * be connected directly to PA8: use a transistor driver.
 */

static uint8_t buzzer_playing = 0;
static uint32_t buzzer_deadline = 0;

uint8_t Button_Read(void)
{
    return (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) ? 1u : 0u;
}

/* Buzzer_Set(1): short 1.8 kHz score chirp, 110 ms
 * Buzzer_Set(2): lower 800 Hz game-over sound, 280 ms
 * Buzzer_Set(0): do not interrupt immediately; stop after deadline.
 * Call Buzzer_Set(0) periodically from the game task.
 */
void Buzzer_Set(uint8_t state)
{
    if (state == 1u || state == 2u)
    {
        const uint32_t period = (state == 1u) ? 556u : 1250u;
        const uint32_t duration = (state == 1u) ? 110u : 280u;

        if (buzzer_playing != 0u)
        {
            (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
            buzzer_playing = 0u;
        }

        __HAL_TIM_SET_PRESCALER(&htim1, 71u);
        __HAL_TIM_SET_AUTORELOAD(&htim1, period - 1u);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, period / 2u);
        __HAL_TIM_SET_COUNTER(&htim1, 0u);
        /* Load the new prescaler before enabling the PWM channel. */
        (void)HAL_TIM_GenerateEvent(&htim1, TIM_EVENTSOURCE_UPDATE);

        if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) == HAL_OK)
        {
            buzzer_deadline = HAL_GetTick() + duration;
            buzzer_playing = 1u;
        }
    }
    else if ((buzzer_playing != 0u) &&
             ((int32_t)(HAL_GetTick() - buzzer_deadline) >= 0))
    {
        (void)HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
        buzzer_playing = 0u;
    }
}
