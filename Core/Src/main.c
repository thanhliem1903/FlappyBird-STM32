/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "hal_button_buzzer.h"
#include "hal_oled.h"
#include "game_logic.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static Bird bird;
static Pipe pipe;
static uint8_t state = 0; /* 0=ready, 1=playing, 2=paused, 3=over */
static uint16_t score = 0, best = 0;
static uint8_t scored = 0;
static uint32_t rng = 0x13579BDFu;

/*
 * PA0 (flap) and PA1 (pause/resume) are active-low with GPIO pull-ups.
 * A press is reported once, only after the signal is stable.
 * The button must be released stably before another press can be reported.
 * Longer release filtering avoids accidental double-presses from contact bounce.
 * Polling is done once per game frame (~33 ms).
 */
#define BUTTON_PRESS_DEBOUNCE_MS     25u
#define BUTTON_RELEASE_DEBOUNCE_MS   70u
#define BUTTON_EVENT_LOCKOUT_MS     130u

typedef struct {
  uint8_t last_raw;       /* last observed GPIO level: 1=released */
  uint8_t stable;         /* filtered level */
  uint8_t armed;          /* a new press can be accepted */
  uint8_t had_event;      /* allow first press without startup lockout */
  uint32_t changed_at;   /* time of last raw level change */
  uint32_t last_event_at; /* time of last accepted press */
} ButtonDebounce;

static ButtonDebounce buttons[2] = {
  {1u, 1u, 1u, 0u, 0u, 0u},
  {1u, 1u, 1u, 0u, 0u, 0u}
};

static uint8_t button_event(uint16_t pin, uint8_t index)
{
  ButtonDebounce *b = &buttons[index];
  const uint8_t raw = (HAL_GPIO_ReadPin(GPIOA, pin) == GPIO_PIN_SET) ? 1u : 0u;
  const uint32_t now = HAL_GetTick();

  if (raw != b->last_raw) {
    b->last_raw = raw;
    b->changed_at = now;
  }

  const uint32_t debounce_ms = raw ? BUTTON_RELEASE_DEBOUNCE_MS
                                   : BUTTON_PRESS_DEBOUNCE_MS;
  if ((raw != b->stable) && ((uint32_t)(now - b->changed_at) >= debounce_ms)) {
    b->stable = raw;

    if (raw == 0u) { /* stable press */
      if (b->armed != 0u &&
          (b->had_event == 0u ||
           (uint32_t)(now - b->last_event_at) >= BUTTON_EVENT_LOCKOUT_MS)) {
        b->armed = 0u;
        b->had_event = 1u;
        b->last_event_at = now;
        return 1u;
      }
      b->armed = 0u; /* even an ignored press must be released before retrigger */
    } else { /* stable release */
      b->armed = 1u;
    }
  }
  return 0u;
}

static void restart_game(void)
{
  Logic_Init(&bird, &pipe);
  score = 0; scored = 0;
  state = 1;
}
/* ---------------- OLED artwork (only uses existing HAL OLED API) ----------- */
/* Compact 5x7 typeface: digits, uppercase A-Z and space. Each byte is a column. */
static const uint8_t ui_digits[10][5] = {
  {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
  {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
  {0x42,0x61,0x51,0x49,0x46}, /* 2 */
  {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
  {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
  {0x27,0x45,0x45,0x45,0x39}, /* 5 */
  {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
  {0x01,0x71,0x09,0x05,0x03}, /* 7 */
  {0x36,0x49,0x49,0x49,0x36}, /* 8 */
  {0x06,0x49,0x49,0x29,0x1E}  /* 9 */
};
static const uint8_t ui_letters[26][5] = {
  {0x7E,0x11,0x11,0x11,0x7E}, /* A */
  {0x7F,0x49,0x49,0x49,0x36}, /* B */
  {0x3E,0x41,0x41,0x41,0x22}, /* C */
  {0x7F,0x41,0x41,0x22,0x1C}, /* D */
  {0x7F,0x49,0x49,0x49,0x41}, /* E */
  {0x7F,0x09,0x09,0x09,0x01}, /* F */
  {0x3E,0x41,0x49,0x49,0x7A}, /* G */
  {0x7F,0x08,0x08,0x08,0x7F}, /* H */
  {0x00,0x41,0x7F,0x41,0x00}, /* I */
  {0x20,0x40,0x41,0x3F,0x01}, /* J */
  {0x7F,0x08,0x14,0x22,0x41}, /* K */
  {0x7F,0x40,0x40,0x40,0x40}, /* L */
  {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
  {0x7F,0x04,0x08,0x10,0x7F}, /* N */
  {0x3E,0x41,0x41,0x41,0x3E}, /* O */
  {0x7F,0x09,0x09,0x09,0x06}, /* P */
  {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
  {0x7F,0x09,0x19,0x29,0x46}, /* R */
  {0x46,0x49,0x49,0x49,0x31}, /* S */
  {0x01,0x01,0x7F,0x01,0x01}, /* T */
  {0x3F,0x40,0x40,0x40,0x3F}, /* U */
  {0x1F,0x20,0x40,0x20,0x1F}, /* V */
  {0x3F,0x40,0x38,0x40,0x3F}, /* W */
  {0x63,0x14,0x08,0x14,0x63}, /* X */
  {0x07,0x08,0x70,0x08,0x07}, /* Y */
  {0x61,0x51,0x49,0x45,0x43}  /* Z */
};

static void ui_character(int16_t x, int16_t y, char ch)
{
  const uint8_t *glyph = NULL;
  if (ch >= '0' && ch <= '9') glyph = ui_digits[ch - '0'];
  else if (ch >= 'A' && ch <= 'Z') glyph = ui_letters[ch - 'A'];
  if (glyph == NULL) return; /* space */
  for (uint8_t col = 0; col < 5; ++col) {
    for (uint8_t row = 0; row < 7; ++row) {
      if ((glyph[col] & (1u << row)) != 0u)
        OLED_DrawPixel(x + col, y + row, 1);
    }
  }
}

static void ui_text(int16_t x, int16_t y, const char *text)
{
  while (*text != '\0') {
    ui_character(x, y, *text++);
    x += 6;
  }
}

static void ui_number(int16_t x, int16_t y, uint16_t value)
{
  char digits[5];
  uint8_t length = 0;
  do {
    digits[length++] = (char)('0' + value % 10u);
    value /= 10u;
  } while (value != 0u && length < sizeof digits);
  while (length > 0u) {
    ui_character(x, y, digits[--length]);
    x += 6;
  }
}

static void ui_frame(int16_t x, int16_t y, int16_t w, int16_t h)
{
  OLED_FillRect(x, y, w, 1, 1);
  OLED_FillRect(x, y + h - 1, w, 1, 1);
  OLED_FillRect(x, y, 1, h, 1);
  OLED_FillRect(x + w - 1, y, 1, h, 1);
}

/* 10x8 silhouette with eye, wing and beak; y is the bird centre. */
static const uint16_t bird_art[8] = {
  0x078u, /* ...####... */
  0x1FCu, /* ..#######. */
  0x3FAu, /* .#####.#.. */
  0x3FFu, /* ########## */
  0x3C7u, /* ###...#### (wing detail) */
  0x1FCu, /* ..#######. */
  0x0F8u, /* ...#####.. */
  0x070u  /* ....###... */
};

static void ui_bird(int16_t x, int16_t y)
{
  for (uint8_t row = 0; row < 8; ++row) {
    for (uint8_t col = 0; col < 10; ++col) {
      if ((bird_art[row] & (1u << (9u - col))) != 0u)
        OLED_DrawPixel(x + col, y - 4 + row, 1);
    }
  }
}

static void ui_pipe(const Pipe *p)
{
  const int16_t top = p->gap_y - GAME_PIPE_HALF_GAP;
  const int16_t bottom = p->gap_y + GAME_PIPE_HALF_GAP;
  const int16_t x = p->x;
  const int16_t w = GAME_PIPE_WIDTH;

  /* Pipe shafts, with thin dark grooves to create a metallic look. */
  OLED_FillRect(x, 0, w, top, 1);
  OLED_FillRect(x, bottom, w, 63 - bottom, 1);
  if (top > 7) OLED_FillRect(x + 2, 2, 1, top - 7, 0);
  if (bottom < 57) OLED_FillRect(x + 2, bottom + 5, 1, 58 - bottom, 0);

  /* 4-pixel rim around each opening. Matches collision in game_logic.c. */
  OLED_FillRect(x - 2, top - 4, w + 4, 4, 1);
  OLED_FillRect(x - 2, bottom, w + 4, 4, 1);
  OLED_FillRect(x - 2, top - 3, w + 4, 1, 0);
  OLED_FillRect(x - 2, bottom + 2, w + 4, 1, 0);
}

static int16_t ui_length(const char *text)
{
  int16_t n = 0;
  while (text[n] != '\0') ++n;
  return n;
}

static void ui_overlay(const char *heading, const char *instruction)
{
  OLED_FillRect(24, 13, 80, 39, 0);
  ui_frame(24, 13, 80, 39);
  ui_text((128 - (int16_t)(6 * ui_length(heading) - 1)) / 2, 22, heading);
  ui_text((128 - (int16_t)(6 * ui_length(instruction) - 1)) / 2,
          38, instruction);
}

static void draw_game(void)
{
  OLED_Clear();

  /* A subtle dotted ground and two clouds behind the game objects. */
  OLED_FillRect(0, 62, 128, 1, 1);
  for (int16_t x = 0; x < 128; x += 9)
    OLED_FillRect(x, 63, 4, 1, 1);
  OLED_FillRect(78, 15, 13, 1, 1);
  OLED_FillRect(81, 13, 6, 1, 1);
  OLED_FillRect(110, 27, 11, 1, 1);
  OLED_FillRect(113, 25, 6, 1, 1);

  ui_pipe(&pipe);
  ui_bird(27, bird.y);

  /* Small high-contrast score display, even when a pipe moves behind it. */
  if (state == 1 || state == 2) {
    OLED_FillRect(0, 0, 74, 10, 0);
    ui_text(2, 1, "SCORE");
    ui_number(39, 1, score);
  }

  if (state == 0) {
    ui_overlay("FLAPPY BIRD", "PA0 START");
  } else if (state == 2) {
    ui_overlay("PAUSED", "PA1 RESUME");
  } else if (state == 3) {
    /* Cover playfield so both scores are readable on the OLED. */
    OLED_FillRect(11, 9, 106, 52, 0);
    ui_frame(11, 9, 106, 52);
    ui_text(37, 15, "GAME OVER");
    ui_text(28, 28, "SCORE");
    ui_number(73, 28, score);   /* Score of the just-finished run */
    ui_text(34, 40, "BEST");
    ui_number(73, 40, best);    /* Highest score since power-on */
    ui_text(37, 52, "PA0 RETRY");
  }

  OLED_Update();
}
void Task_Game(void *arg)
{
  (void)arg;
  TickType_t last = xTaskGetTickCount();
  Logic_Init(&bird, &pipe);
  OLED_Init();
  for (;;) {
    uint8_t jump = button_event(GPIO_PIN_0, 0);
    uint8_t pause = button_event(GPIO_PIN_1, 1);
    if (pause) {
      if (state == 1) state = 2;
      else if (state == 2) state = 1;
    }
    if (jump) {
      if (state == 0 || state == 3) { restart_game(); Logic_Flap(&bird); }
      else if (state == 1) Logic_Flap(&bird);
    }
    if (state == 1) {
      Logic_Update(&bird, &pipe, 0);
      if (!scored && pipe.x + GAME_PIPE_WIDTH + 2 < 27) {
        ++score; scored = 1;
        Buzzer_Set(1);
      } else Buzzer_Set(0);
      if (pipe.x <= -GAME_PIPE_WIDTH - 2) {
        rng = rng * 1664525u + 1013904223u;
        pipe.x = 128; pipe.gap_y = 24 + (int16_t)(rng % 16u);
        scored = 0;
      }
      if (Logic_CheckCollision(&bird, &pipe)) {
        state = 3;
        if (score > best) best = score;
        Buzzer_Set(2); /* Longer low-pitched game-over sound. */
      }
    } else Buzzer_Set(0);
    draw_game();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(33));
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  /* Game task owns both logic and OLED to prevent concurrent framebuffer access. */
  xTaskCreate(Task_Game, "Game", 384, NULL, 2, NULL);

  /* USER CODE END 2 */

  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
