#include <stm32g0xx_hal.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "elli_fit.h"

static uint8_t swv_buf[256];
static size_t swv_buf_ptr = 0;
__attribute__ ((noinline, used))
void swv_trap_line()
{
  *(volatile char *)swv_buf;
}
static inline void swv_putchar(uint8_t c)
{
  // ITM_SendChar(c);
  if (c == '\n') {
    swv_buf[swv_buf_ptr >= sizeof swv_buf ?
      (sizeof swv_buf - 1) : swv_buf_ptr] = '\0';
    swv_trap_line();
    swv_buf_ptr = 0;
  } else if (++swv_buf_ptr <= sizeof swv_buf) {
    swv_buf[swv_buf_ptr - 1] = c;
  }
}
static void swv_printf(const char *restrict fmt, ...)
{
  char s[256];
  va_list args;
  va_start(args, fmt);
  int r = vsnprintf(s, sizeof s, fmt, args);
  for (int i = 0; i < r && i < sizeof s - 1; i++) swv_putchar(s[i]);
  if (r >= sizeof s) {
    for (int i = 0; i < 3; i++) swv_putchar('.');
    swv_putchar('\n');
  }
}

I2S_HandleTypeDef i2s1;
SPI_HandleTypeDef spi2;

void setup_clocks()
{
  RCC_OscInitTypeDef osc_init = { 0 };
  osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  osc_init.HSEState = RCC_HSE_ON;     // 32 MHz
  osc_init.PLL.PLLState = RCC_PLL_ON;
  osc_init.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  osc_init.PLL.PLLM = RCC_PLLM_DIV2;  // VCO input 16 MHz (2.66 ~ 16 MHz)
  osc_init.PLL.PLLN = 8;              // VCO output 128 MHz (64 ~ 344 MHz)
  osc_init.PLL.PLLP = RCC_PLLP_DIV2;  // PLLPCLK 64 MHz
  osc_init.PLL.PLLR = RCC_PLLR_DIV2;  // PLLRCLK 64 MHz
  HAL_RCC_OscConfig(&osc_init);

  RCC_ClkInitTypeDef clk_init = { 0 };
  clk_init.ClockType =
    RCC_CLOCKTYPE_SYSCLK |
    RCC_CLOCKTYPE_HCLK |
    RCC_CLOCKTYPE_PCLK1;
  clk_init.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK; // 64 MHz
  clk_init.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk_init.APB1CLKDivider = RCC_HCLK_DIV1;
  HAL_RCC_ClockConfig(&clk_init, FLASH_LATENCY_2);
}

#pragma GCC push_options
#pragma GCC optimize("O3")
static inline void led_write(const uint32_t *data, size_t n)
{
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 1);
  __disable_irq();

  // Clear zero
  for (int j = 0; j < 32; j++) {
    GPIOA->BSRR = (1 << (7 + 16)) | (1 << (5 + 16));
    asm volatile ("nop");
    GPIOA->BSRR = 1 << 7;
  }

  for (int i = 0; i < n; i++)
    #pragma GCC unroll 32
    for (int j = 0; j < 32; j++) {
      uint8_t b = (data[i] >> (31 - j)) & 1;
      GPIOA->BSRR = (1 << (7 + 16)) |
        (b ? (1 << 5) : (1 << (5 + 16)));
      asm volatile ("nop");
      GPIOA->BSRR = 1 << 7;
    }

  // Clear one
  for (int j = 0; j < 32; j++) {
    GPIOA->BSRR = (1 << (7 + 16)) | (1 << 5);
    asm volatile ("nop");
    GPIOA->BSRR = 1 << 7;
  }

  __enable_irq();
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 0);
}
#pragma GCC pop_options

static inline void led_flush()
{
  uint32_t data[24];
  for (int i = 0; i < 24; i++) data[i] = 0xe0000000;
  led_write(data, 24);
}

#include "lcd.h"

const uint8_t Tamzen7x14[95][14] = {
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x10, 0x10, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x28, 0x28, 0x28, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x28, 0x28, 0x7c, 0x28, 0x28, 0x7c, 0x28, 0x28, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x10, 0x10, 0x3c, 0x40, 0x40, 0x38, 0x04, 0x04, 0x78, 0x10, 0x10},
  {0x00, 0x00, 0x00, 0x00, 0x40, 0xa4, 0xa8, 0x50, 0x28, 0x54, 0x94, 0x08, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x20, 0x50, 0x50, 0x50, 0x24, 0x54, 0x48, 0x4c, 0x32, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x08, 0x10, 0x10, 0x20, 0x20, 0x20, 0x20, 0x20, 0x10, 0x10, 0x08},
  {0x00, 0x00, 0x00, 0x20, 0x10, 0x10, 0x08, 0x08, 0x08, 0x08, 0x08, 0x10, 0x10, 0x20},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x54, 0x38, 0x54, 0x10, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x10, 0x7c, 0x10, 0x10, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x10, 0x10},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x04, 0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x40, 0x40, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x4c, 0x54, 0x64, 0x44, 0x44, 0x38, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x10, 0x30, 0x50, 0x10, 0x10, 0x10, 0x10, 0x7c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x04, 0x08, 0x10, 0x20, 0x40, 0x7c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x7c, 0x04, 0x08, 0x18, 0x04, 0x04, 0x44, 0x38, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x08, 0x18, 0x28, 0x48, 0x7c, 0x08, 0x08, 0x08, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x7c, 0x40, 0x40, 0x78, 0x04, 0x04, 0x44, 0x38, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x18, 0x20, 0x40, 0x78, 0x44, 0x44, 0x44, 0x38, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x7c, 0x04, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x44, 0x38, 0x44, 0x44, 0x44, 0x38, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x44, 0x44, 0x3c, 0x04, 0x08, 0x30, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00, 0x00, 0x30, 0x30, 0x10, 0x10},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x20, 0x10, 0x08, 0x10, 0x20, 0x40, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x38, 0x44, 0x04, 0x08, 0x10, 0x00, 0x00, 0x10, 0x10, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x44, 0x4c, 0x54, 0x5c, 0x40, 0x40, 0x3c, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x10, 0x28, 0x44, 0x44, 0x7c, 0x44, 0x44, 0x44, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x78, 0x44, 0x44, 0x78, 0x44, 0x44, 0x44, 0x78, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x1c, 0x20, 0x40, 0x40, 0x40, 0x40, 0x20, 0x1c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x78, 0x44, 0x44, 0x44, 0x44, 0x44, 0x48, 0x70, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x7c, 0x40, 0x40, 0x78, 0x40, 0x40, 0x40, 0x7c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x7c, 0x40, 0x40, 0x78, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x1c, 0x20, 0x40, 0x40, 0x4c, 0x44, 0x24, 0x1c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x44, 0x7c, 0x44, 0x44, 0x44, 0x44, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x7c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x7c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x04, 0x44, 0x44, 0x38, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x44, 0x48, 0x50, 0x60, 0x60, 0x50, 0x48, 0x44, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x44, 0x6c, 0x54, 0x54, 0x44, 0x44, 0x44, 0x44, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x44, 0x64, 0x54, 0x4c, 0x44, 0x44, 0x44, 0x44, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x78, 0x44, 0x44, 0x44, 0x78, 0x40, 0x40, 0x40, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38, 0x08, 0x04},
  {0x00, 0x00, 0x00, 0x00, 0x78, 0x44, 0x44, 0x44, 0x78, 0x50, 0x48, 0x44, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x3c, 0x40, 0x40, 0x30, 0x08, 0x04, 0x04, 0x78, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x7c, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x44, 0x38, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44, 0x28, 0x28, 0x10, 0x10, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44, 0x54, 0x54, 0x54, 0x6c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x28, 0x10, 0x10, 0x28, 0x44, 0x44, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x28, 0x28, 0x10, 0x10, 0x10, 0x10, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x7c, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x7c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38},
  {0x00, 0x00, 0x00, 0x40, 0x40, 0x20, 0x20, 0x10, 0x10, 0x08, 0x08, 0x04, 0x04, 0x00},
  {0x00, 0x00, 0x00, 0x38, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x38},
  {0x00, 0x00, 0x00, 0x00, 0x10, 0x28, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe},
  {0x00, 0x00, 0x00, 0x20, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x04, 0x3c, 0x44, 0x44, 0x3c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x58, 0x64, 0x44, 0x44, 0x44, 0x78, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x40, 0x40, 0x40, 0x40, 0x3c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x3c, 0x44, 0x44, 0x44, 0x44, 0x3c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x7c, 0x40, 0x40, 0x3c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x1c, 0x20, 0x7c, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x44, 0x44, 0x44, 0x44, 0x3c, 0x04, 0x38},
  {0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x58, 0x64, 0x44, 0x44, 0x44, 0x44, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x10, 0x10, 0x00, 0x70, 0x10, 0x10, 0x10, 0x10, 0x7c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x08, 0x08, 0x00, 0x38, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x70},
  {0x00, 0x00, 0x00, 0x40, 0x40, 0x40, 0x48, 0x50, 0x60, 0x50, 0x48, 0x44, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x70, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x0c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x54, 0x54, 0x54, 0x54, 0x54, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x64, 0x44, 0x44, 0x44, 0x44, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x44, 0x44, 0x44, 0x44, 0x38, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x64, 0x44, 0x44, 0x44, 0x78, 0x40, 0x40},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x44, 0x44, 0x44, 0x4c, 0x34, 0x04, 0x04},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5c, 0x60, 0x40, 0x40, 0x40, 0x40, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x40, 0x30, 0x08, 0x04, 0x78, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x20, 0x20, 0x7c, 0x20, 0x20, 0x20, 0x20, 0x1c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44, 0x44, 0x3c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x28, 0x28, 0x10, 0x10, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x54, 0x54, 0x54, 0x6c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x28, 0x10, 0x10, 0x28, 0x44, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x44, 0x44, 0x44, 0x4c, 0x34, 0x04, 0x04},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7c, 0x08, 0x10, 0x10, 0x20, 0x7c, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x0c, 0x10, 0x10, 0x10, 0x10, 0x60, 0x10, 0x10, 0x10, 0x10, 0x0c},
  {0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00},
  {0x00, 0x00, 0x00, 0x60, 0x10, 0x10, 0x10, 0x10, 0x0c, 0x10, 0x10, 0x10, 0x10, 0x60},
  {0x00, 0x00, 0x00, 0x00, 0x24, 0x54, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};
static inline void lcd_print_char(uint8_t ch, int r, int c)
{
  lcd_addr(c, r, c + 7, r + 13);
  for (int dr = 0; dr < 14; dr++)
    for (int dc = 0; dc < 8; dc++)
      lcd_data16((Tamzen7x14[ch - 32][dr] >> (7 - dc)) & 1 ? 0xffff : 0x0000);
}
static inline void lcd_print_str(const char *s, int r, int c)
{
  int c0 = c;
  for (; *s != '\0'; s++) {
    if (*s == '\n') {
      r += 14;
      c = c0;
    } else {
      lcd_print_char(*s, r, c);
      c += 8;
      if (c >= 240) {
        r += 14;
        if (r >= 240) r = 0;
        c = 0;
      }
    }
  }
}

#define norm(_x, _y) ((_x) * (_x) + (_y) * (_y))

typedef struct __attribute__ ((packed, aligned(4))) { int16_t l, r; } stereo_sample_t;
static inline stereo_sample_t sample(int16_t x) { return (stereo_sample_t){x, x}; }
// Buffer for audio samples
#define N_AUDIO_PCM_BUF 240
static stereo_sample_t audio_pcm_buf[N_AUDIO_PCM_BUF] = { 0 };

#define TILE_S 20
#define TILE_N (240 / TILE_S)
static uint8_t tile_pixels[TILE_S * TILE_S * 2];

#define TILE_BITS ((TILE_N * TILE_N + 7) / 8)
typedef struct screen_element {
  void (*update)(struct screen_element *self, uint8_t *dirty);
  void (*fill_tile)(struct screen_element *self, uint32_t x0, uint32_t y0, uint8_t *pixels);
} screen_element;

static screen_element *screen_elements[16] = { 0 };
static uint8_t n_screen_elements = 0;

#pragma GCC push_options
#pragma GCC optimize("O3")
#define rgb565(_r, _g, _b) ( \
  ((uint16_t)((_g) & 0b00011100) << 11) | \
  ((uint16_t)((_b) & 0b11111000) <<  5) | \
  ((uint16_t)((_r) & 0b11111000) <<  0) | \
  ((uint16_t)((_g) & 0b11100000) >>  5))

static uint32_t max_lx = 0;
static uint8_t lx_levels[24] = { 0 };
static uint8_t lx_levels_disp[24] = { 0 };
static void outer_ring_update(screen_element *restrict self, uint8_t *restrict dirty)
{
  static const uint8_t ring_tiles[24][3][2] = {
    {{ 0, 0b11000000}, {-1}},
    {{ 0, 0b10000000}, { 1, 0b00000001}, { 2, 0b00011000}},
    {{ 2, 0b01110000}, { 4, 0b00000010}, {-1}},
    {{ 2, 0b01000000}, { 4, 0b00000110}, { 5, 0b01000000}},
    {{ 5, 0b11000000}, { 7, 0b00001100}, {-1}},
    {{ 7, 0b00001000}, { 8, 0b10000000}, {-1}},
    {{10, 0b00001000}, {11, 0b10000000}, {-1}},
    {{11, 0b11000000}, {13, 0b00001100}, {-1}},
    {{13, 0b00000100}, {14, 0b01100000}, {16, 0b00000100}},
    {{14, 0b00100000}, {16, 0b00000111}, {-1}},
    {{15, 0b10000000}, {16, 0b00000001}, {17, 0b00011000}},
    {{17, 0b00001100}, {-1}},
    {{17, 0b00000011}, {-1}},
    {{15, 0b00011000}, {16, 0b10000000}, {17, 0b00000001}},
    {{13, 0b01000000}, {15, 0b00001110}, {-1}},
    {{12, 0b00000010}, {13, 0b01100000}, {15, 0b00000010}},
    {{10, 0b00110000}, {12, 0b00000011}, {-1}},
    {{ 9, 0b00000001}, {10, 0b00010000}, {-1}},
    {{ 6, 0b00000001}, { 7, 0b00010000}, {-1}},
    {{ 4, 0b00110000}, { 6, 0b00000011}, {-1}},
    {{ 1, 0b00100000}, { 3, 0b00000110}, { 4, 0b00100000}},
    {{ 1, 0b11100000}, { 3, 0b00000100}, {-1}},
    {{ 0, 0b00011000}, { 1, 0b10000000}, { 2, 0b00000001}},
    {{ 0, 0b00110000}, {-1}},
  };
  for (int i = 0; i < 24; i++) {
    if (lx_levels_disp[i] != lx_levels[i]) {
      lx_levels_disp[i] = lx_levels[i];
      for (int j = 0; j < 3 && ring_tiles[i][j][0] != (uint8_t)-1; j++)
        dirty[ring_tiles[i][j][0]] |= ring_tiles[i][j][1];
    }
  }
}
static void outer_ring_fill_tile(struct screen_element *self, uint32_t x0, uint32_t y0, uint8_t *pixels)
{
  for (int dy = 0; dy < TILE_S; dy++)
    for (int dx = 0; dx < TILE_S; dx++) {
      uint32_t i = (dy * TILE_S + dx) * 2;
      uint32_t x = x0 + dx, y = y0 + dy;
      // Coordinates translated w.r.t. centre
      int32_t xc = x * 2 + 1 - 240;
      int32_t yc = y * 2 + 1 - 240;
      uint32_t dsq = norm(xc, yc);
      if (dsq >= 4 * 106 * 106 && dsq <= 4 * 116 * 116) {
        // Find which 24-ant
        // (1) Normalise to the first quadrant
        uint32_t div = 0;
        int32_t swap_t;
        if (xc >= 0) {
          if (yc >= 0) { div = 6; }
          else { div = 0; swap_t = xc; xc = -yc; yc = swap_t; }
        } else {
          if (yc >= 0) { div = 12; swap_t = xc; xc = yc; yc = -swap_t; }
          else { div = 18; xc = -xc; yc = -yc; }
        }
        // (2) Normalise to the first octant
        bool flip = false;
        if (yc > xc) {
          swap_t = xc; xc = yc; yc = swap_t;
          flip = true;
        }
        // (3) Discriminate between the 3 subdivisions
        int32_t div3 = 0;
        if (3 * yc * yc >= xc * xc) div3 = 2;
        else if (780 * yc * yc >= 56 * xc * xc) div3 = 1;

        div += (flip ? (5 - div3) : div3);

        if (lx_levels_disp[div] == 3) {
          *(uint16_t *)(pixels + i) = rgb565(0xc0, 0x80, 0x20);
        } else if (lx_levels_disp[div] == 2) {
          *(uint16_t *)(pixels + i) = rgb565(0x60, 0x40, 0x10);
        } else if (lx_levels_disp[div] == 1) {
          *(uint16_t *)(pixels + i) = rgb565(0x20, 0x18, 0x00);
        } else {
          *(uint16_t *)(pixels + i) = 0x0000;
        }
      } else {
        *(uint16_t *)(pixels + i) = 0x0000;
      }
    }
}

static uint32_t compass_x = 0, compass_y = 0;
static uint32_t compass_x_disp = 0, compass_y_disp = 0;
static void compass_update(screen_element *restrict self, uint8_t *restrict dirty)
{
  uint32_t last_tile_x_min = (compass_x_disp / 256 - 7) / TILE_S;
  uint32_t last_tile_x_max = (compass_x_disp / 256 + 7) / TILE_S;
  uint32_t last_tile_y_min = (compass_y_disp / 256 - 7) / TILE_S;
  uint32_t last_tile_y_max = (compass_y_disp / 256 + 7) / TILE_S;
  for (uint32_t y = last_tile_y_min; y <= last_tile_y_max; y++)
    for (uint32_t x = last_tile_x_min; x <= last_tile_x_max; x++) {
      uint32_t tile_num = y * TILE_N + x;
      dirty[tile_num / 8] |= (1 << (tile_num % 8));
    }

  uint32_t tile_x_min = (compass_x / 256 - 7) / TILE_S;
  uint32_t tile_x_max = (compass_x / 256 + 7) / TILE_S;
  uint32_t tile_y_min = (compass_y / 256 - 7) / TILE_S;
  uint32_t tile_y_max = (compass_y / 256 + 7) / TILE_S;
  for (uint32_t y = tile_y_min; y <= tile_y_max; y++)
    for (uint32_t x = tile_x_min; x <= tile_x_max; x++) {
      uint32_t tile_num = y * TILE_N + x;
      dirty[tile_num / 8] |= (1 << (tile_num % 8));
    }

  compass_x_disp = compass_x;
  compass_y_disp = compass_y;
}
static void compass_fill_tile(struct screen_element *self, uint32_t x0, uint32_t y0, uint8_t *pixels)
{
  for (int dy = 0; dy < TILE_S; dy++)
    for (int dx = 0; dx < TILE_S; dx++) {
      uint32_t i = (dy * TILE_S + dx) * 2;
      uint32_t x = x0 + dx, y = y0 + dy;
      uint32_t n = norm(x * 256 - compass_x_disp, y * 256 - compass_y_disp);
      if (n < 38 * 256 * 256) {
        *(uint16_t *)(tile_pixels + i) = rgb565(0xff, 0xc0, 0x20);
      } else if (n < 40 * 256 * 256) {
        uint32_t rate = (n - 35 * 256 * 256) / (5 * 256);
        uint32_t r = rate * 0xff / 256;
        uint32_t g = rate * 0xc0 / 256;
        uint32_t b = rate * 0x20 / 256;
        *(uint16_t *)(pixels + i) = rgb565(r, g, b);
      }
    }
}

typedef struct {
  screen_element _base;
  uint32_t x, y;
  const char *s;
  uint8_t last_tiles[TILE_BITS];
} screen_element_text;
static void text_update(screen_element *restrict _self, uint8_t *restrict dirty)
{
  screen_element_text *self = (screen_element_text *)_self;
  for (int i = 0; i < TILE_BITS; i++) dirty[i] |= self->last_tiles[i];
  // Fill new
  for (int i = 0; i < TILE_BITS; i++) self->last_tiles[i] = 0;
  uint32_t x = self->x, y = self->y;
  for (const char *s = self->s; *s != '\0'; s++) {
    if (*s == '\n') {
      y += 15;
      x = self->x;
    } else {
      uint32_t tile_x_min = x / TILE_S;
      uint32_t tile_x_max = (x + 7) / TILE_S;
      uint32_t tile_y_min = y / TILE_S;
      uint32_t tile_y_max = (y + 14) / TILE_S;
      for (uint32_t y = tile_y_min; y <= tile_y_max; y++)
        for (uint32_t x = tile_x_min; x <= tile_x_max; x++) {
          uint32_t tile_num = y * TILE_N + x;
          self->last_tiles[tile_num / 8] |= (1 << (tile_num % 8));
        }
      x += 8;
    }
  }
  // Blit to the main bitmap
  for (int i = 0; i < TILE_BITS; i++) dirty[i] |= self->last_tiles[i];
}
static void text_fill_tile(struct screen_element *_self, uint32_t x0, uint32_t y0, uint8_t *pixels)
{
  screen_element_text *self = (screen_element_text *)_self;
  uint32_t x = self->x, y = self->y;
  for (const char *s = self->s; *s != '\0'; s++) {
    if (*s == '\n') {
      y += 15;
      x = self->x;
    } else {
      for (int dr = 0; dr < 14; dr++) {
        int32_t y1 = y + dr - y0;
        if (y1 < 0 || y1 >= TILE_S) continue;
        for (int dc = 0; dc < 8; dc++) {
          int32_t x1 = x + dc - x0;
          if (x1 < 0 || x1 >= TILE_S) continue;
          uint32_t i = (y1 * TILE_S + x1) * 2;
          if ((Tamzen7x14[*s - 32][dr] >> (7 - dc)) & 1)
            *(uint16_t *)(pixels + i) = rgb565(0xff, 0xff, 0xff);
        }
      }
      x += 8;
    }
  }
}
static screen_element_text text_create()
{
  screen_element_text o = { 0 };
  _Static_assert((void *)&o._base == (void *)&o, "Base field reordered");
  o._base.update = &text_update;
  o._base.fill_tile = &text_fill_tile;
  return o;
}

static void lcd_fill(uint8_t byte)
{
  lcd_addr(0, 0, 239, 239);
  lcd_cs(0);
  for (int i = 0; i < 240 * 240 * 2; i++) {
    while (!(SPI2->SR & SPI_SR_TXE)) { }
    SPI2->DR = byte;
  }
  while (!(SPI2->SR & SPI_SR_TXE)) { }
  while ((SPI2->SR & SPI_SR_BSY)) { }
  // Clear OVR flag
  (void)SPI2->DR;
  (void)SPI2->SR;
  lcd_cs(1);
}

static volatile bool lcd_dma_busy = false;
static uint8_t lcd_dirty[TILE_BITS];
static void lcd_new_frame()
{
  for (int i = 0; i < TILE_BITS; i++) lcd_dirty[i] = 0;
  for (int i = 0; i < n_screen_elements; i++)
    screen_elements[i]->update(screen_elements[i], lcd_dirty);
}
static void lcd_next_tile(uint32_t tile_num)
{
  if (!(lcd_dirty[tile_num / 8] & (1 << (tile_num % 8)))) return;
  uint32_t x0 = tile_num % TILE_N * TILE_S;
  uint32_t y0 = tile_num / TILE_N * TILE_S;
  for (int j = 0; j < n_screen_elements; j++)
    screen_elements[j]->fill_tile(screen_elements[j], x0, y0, tile_pixels);
  while (SPI2->SR & SPI_SR_BSY) { }
  lcd_addr(x0, y0, x0 + TILE_S - 1, y0 + TILE_S - 1);
  lcd_dma_busy = true;
  lcd_data_bulk_dma(tile_pixels, TILE_S * TILE_S * 2);
}
#pragma GCC pop_options

#pragma GCC push_options
#pragma GCC optimize("O3")
static void i2c_delay()
{
  // 10 us = 100 kHz
  // NOTE: Lower frequency might be used with internal weak pull-ups and/or for debugging
  static const uint32_t us = 20;
  for (int i = 0; i < 64 * us / 4; i++) asm volatile ("nop");
}
static bool read_SCL() { return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10); }
static void set_SCL() { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET); }
static void clear_SCL() { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET); }

static uint32_t (*read_SDA)();
static void (*write_SDA)();

static uint32_t _read_SDA() {
  return
    (((GPIOB->IDR >> 10) & 1) <<  0) |
    (((GPIOB->IDR >> 11) & 1) <<  1) |
    (((GPIOB->IDR >> 12) & 1) <<  2) |
    (((GPIOB->IDR >> 13) & 1) <<  3) |
    (((GPIOB->IDR >> 14) & 1) <<  4) |
    (((GPIOA->IDR >>  0) & 1) <<  5) |
    (((GPIOA->IDR >>  1) & 1) <<  6) |
    (((GPIOA->IDR >>  2) & 1) <<  7) |
    (((GPIOA->IDR >>  3) & 1) <<  8) |
    (((GPIOB->IDR >>  0) & 1) <<  9) |
    (((GPIOB->IDR >>  1) & 1) << 10) |
    (((GPIOB->IDR >>  2) & 1) << 11) |
    (0xfffff000);
}
static void _write_SDA(uint32_t value) {
  uint32_t mask_a = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
  GPIOA->ODR = (GPIOA->ODR & ~mask_a)
    | (((value >>  5) & 1) <<  0)
    | (((value >>  6) & 1) <<  1)
    | (((value >>  7) & 1) <<  2)
    | (((value >>  8) & 1) <<  3)
    ;
  uint32_t mask_b = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 10) | (1 << 11) | (1 << 12) | (1 << 13) | (1 << 14);
  GPIOB->ODR = (GPIOB->ODR & ~mask_b)
    | (((value >>  9) & 1) <<  0)
    | (((value >> 10) & 1) <<  1)
    | (((value >> 11) & 1) <<  2)
    | (((value >>  0) & 1) << 10)
    | (((value >>  1) & 1) << 11)
    | (((value >>  2) & 1) << 12)
    | (((value >>  3) & 1) << 13)
    | (((value >>  4) & 1) << 14)
    ;
}

static uint32_t _read_SDA_04() {
  return ((GPIOB->IDR >> 14) & 1) | 0xfffffffe;
}
static void _write_SDA_04(uint32_t value) {
  GPIOB->BSRR = (value & 1) ? (1 << 14) : (1 << 30);
}

static uint32_t _read_SDA_06() {
  return ((GPIOA->IDR >> 1) & 1) | 0xfffffffe;
}
static void _write_SDA_06(uint32_t value) {
  GPIOA->BSRR = (value & 1) ? (1 << 1) : (1 << 17);
}

// 0 - no error
// 1 - no acknowledgement
// 2 - timeout
// 4 - bus error, arbitration lost
static int i2c_err = 0;
static int i2c_first_err_line = -1;

static void i2c_mark_err(int line, int flag)
{
  if (i2c_err == 0) i2c_first_err_line = line;
  i2c_err |= flag;
}

static void wait_SCL_rise(int line)
{
  for (int i = 0; i < 2; i++) {
    i2c_delay();
    if (read_SCL()) return;
  }
  i2c_mark_err(line, 2);
}

static bool started = false;

static void i2c_init()
{
  clear_SCL();
  write_SDA(0x00000000);
}

static void i2c_start_cond(void)
{
  write_SDA(0xffffffff);
  i2c_delay();

  set_SCL();
  wait_SCL_rise(__LINE__);
  i2c_delay();

  if (read_SDA() != 0xffffffff) {
    i2c_mark_err(__LINE__, 4);
    return;
  }

  write_SDA(0x00000000);
  i2c_delay();

  clear_SCL();
  started = true;
}

static void i2c_stop_cond(void)
{
  write_SDA(0x00000000);
  i2c_delay();

  set_SCL();
  wait_SCL_rise(__LINE__);

  i2c_delay();

  write_SDA(0xffffffff);
  i2c_delay();

  if (read_SDA() != 0xffffffff) {
    i2c_mark_err(__LINE__, 4);
    return;
  }

  started = false;
}

// Write a bit to I2C bus
static void i2c_write_bit(const uint8_t *a, int bit)
{
  uint32_t bitset = 0;
  for (int i = 0; i < 12; i++)
    bitset |= (((a[i] >> bit) & 1) << i);

  write_SDA(bitset);

  i2c_delay();
  set_SCL();

  i2c_delay();
  wait_SCL_rise(__LINE__);

  if ((read_SDA() & bitset) != bitset) {
    i2c_mark_err(__LINE__, 4);
    return;
  }

  clear_SCL();
}

static void i2c_write_bit_all(bool bit)
{
  if (bit) write_SDA(0xffffffff);
  else write_SDA(0x00000000);

  i2c_delay();
  set_SCL();

  i2c_delay();
  wait_SCL_rise(__LINE__);

  if (bit && read_SDA() != 0xffffffff) {
    i2c_mark_err(__LINE__, 4);
    return;
  }

  clear_SCL();
}

// Read a bit from I2C bus
static void i2c_read_bit(uint8_t *a)
{
  write_SDA(0xffffffff);
  i2c_delay();

  set_SCL();
  wait_SCL_rise(__LINE__);

  i2c_delay();
  uint32_t bitset = read_SDA();
  clear_SCL();

  for (int i = 0; i < 12; i++)
    a[i] = (a[i] << 1) | ((bitset >> i) & 1);
}

static bool i2c_read_nack()
{
  write_SDA(0xffffffff);
  i2c_delay();

  set_SCL();
  wait_SCL_rise(__LINE__);

  i2c_delay();
  uint32_t sda = read_SDA();
  bool bit = (sda == 0xffffffff);
  // Debug use
  if (!bit && read_SDA == _read_SDA && sda != 0xfffff000) swv_printf("ACK %08x\n", read_SDA());
  clear_SCL();
  return bit;
}

// Write a byte to I2C bus. Return 0 if ACK'ed by the target.
static bool i2c_write_byte(bool send_start, bool send_stop, const uint8_t *byte)
{
  if (send_start) i2c_start_cond();
  for (int bit = 0; bit < 8; ++bit)
    i2c_write_bit(byte, 7 - bit);
  bool nack = i2c_read_nack();
  if (send_stop) i2c_stop_cond();
  if (nack) i2c_mark_err(__LINE__, 1);
  return nack;
}

static bool i2c_write_byte_all(bool send_start, bool send_stop, uint8_t byte)
{
  if (send_start) i2c_start_cond();
  for (int bit = 0; bit < 8; ++bit)
    i2c_write_bit_all((byte >> (7 - bit)) & 1);
  bool nack = i2c_read_nack();
  if (send_stop) i2c_stop_cond();
  if (nack) i2c_mark_err(__LINE__, 1);
  return nack;
}

// Read a byte from I2C bus
static void i2c_read_byte(bool nack, bool send_stop, uint8_t *byte)
{
  for (int bit = 0; bit < 8; ++bit) i2c_read_bit(byte);
  i2c_write_bit_all(nack);
  if (send_stop) i2c_stop_cond();
}

static void i2c_write(uint8_t addr, const uint8_t *data, size_t size)
{
  i2c_write_byte_all(true, false, addr);
  for (size_t i = 0; i < size; i++)
    i2c_write_byte_all(false, (i == size - 1), data[i]);
}

static void i2c_read(uint8_t addr, uint8_t *buf, size_t size)
{
  i2c_write_byte_all(true, false, addr | 1);
  for (size_t i = 0; i < size; i++)
    i2c_read_byte(i == size - 1, i == size - 1, buf + 12 * i);
}

static void i2c_write_reg_byte(uint8_t addr, uint8_t reg, uint8_t data)
{
  i2c_write_byte_all(true, false, addr);
  i2c_write_byte_all(false, false, reg);
  i2c_write_byte_all(false, true, data);
}

static void i2c_read_reg(uint8_t addr, uint8_t reg, size_t size, uint8_t *buf)
{
  i2c_write_byte_all(true, false, addr);
  i2c_write_byte_all(false, false, reg);
  i2c_write_byte_all(true, false, addr | 1);
  uint8_t t[12];
  for (size_t i = 0; i < size; i++) {
    i2c_read_byte(i == size - 1, i == size - 1, t);
    buf[i] = t[0];
  }
}
#pragma GCC pop_options
// End of I2C

static inline void bh1750fvi_readout_start(uint8_t addr)
{
  // 0x1_ - Continuous
  // 0x2_ - One Time
  // 0x_0 - H-Resolution Mode
  // 0x_1 - H-Resolution Mode2
  // 0x_3 - L-Resolution Mode
  uint8_t op = 0x13;
  i2c_write(addr, &op, 1);
}
static inline void bh1750fvi_readout(uint8_t addr, uint16_t results[12])
{
  uint8_t result[24];
  i2c_read(addr, result, 2);
  for (int i = 0; i < 12; i++) {
    uint16_t lx = ((uint16_t)result[i] << 8) | result[i + 12];
    lx = (lx * 5 + 3) / 6;
    results[i] = lx;
  }
}

static inline uint16_t max17049_read_reg(uint8_t reg)
{
  uint8_t value[2];
  i2c_read_reg(0b01101100, reg, 2, value);
  return ((uint16_t)value[0] << 8) | value[1];
}

static inline uint8_t bmi270_read_reg(uint8_t reg)
{
  uint8_t value;
  i2c_read_reg(0b11010000, reg, 1, &value);
  return value;
}
static inline void bmi270_write_reg(uint8_t reg, uint8_t value)
{
  i2c_write_reg_byte(0b11010000, reg, value);
}
static inline void bmi270_read_burst(uint8_t reg, uint8_t *data, uint32_t len)
{
  i2c_read_reg(0b11010000, reg, len, data);
}
static inline void bmi270_write_burst(const uint8_t *data, uint32_t len)
{
  i2c_write(0b11010000, data, len);
}
static const uint8_t bmi270_config_file[] = {
  0x5E,
  // bmi270_maximum_fifo_config_file
  0xc8, 0x2e, 0x00, 0x2e, 0x80, 0x2e, 0x1a, 0x00, 0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00,
  0x2e, 0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00, 0x2e, 0xc8, 0x2e, 0x00, 0x2e, 0x90, 0x32, 0x21, 0x2e, 0x59, 0xf5,
  0x10, 0x30, 0x21, 0x2e, 0x6a, 0xf5, 0x1a, 0x24, 0x22, 0x00, 0x80, 0x2e, 0x3b, 0x00, 0xc8, 0x2e, 0x44, 0x47, 0x22,
  0x00, 0x37, 0x00, 0xa4, 0x00, 0xff, 0x0f, 0xd1, 0x00, 0x07, 0xad, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1,
  0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00,
  0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x80, 0x2e, 0x00, 0xc1, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x11, 0x24, 0xfc, 0xf5, 0x80, 0x30, 0x40, 0x42, 0x50, 0x50, 0x00, 0x30, 0x12, 0x24, 0xeb,
  0x00, 0x03, 0x30, 0x00, 0x2e, 0xc1, 0x86, 0x5a, 0x0e, 0xfb, 0x2f, 0x21, 0x2e, 0xfc, 0xf5, 0x13, 0x24, 0x63, 0xf5,
  0xe0, 0x3c, 0x48, 0x00, 0x22, 0x30, 0xf7, 0x80, 0xc2, 0x42, 0xe1, 0x7f, 0x3a, 0x25, 0xfc, 0x86, 0xf0, 0x7f, 0x41,
  0x33, 0x98, 0x2e, 0xc2, 0xc4, 0xd6, 0x6f, 0xf1, 0x30, 0xf1, 0x08, 0xc4, 0x6f, 0x11, 0x24, 0xff, 0x03, 0x12, 0x24,
  0x00, 0xfc, 0x61, 0x09, 0xa2, 0x08, 0x36, 0xbe, 0x2a, 0xb9, 0x13, 0x24, 0x38, 0x00, 0x64, 0xbb, 0xd1, 0xbe, 0x94,
  0x0a, 0x71, 0x08, 0xd5, 0x42, 0x21, 0xbd, 0x91, 0xbc, 0xd2, 0x42, 0xc1, 0x42, 0x00, 0xb2, 0xfe, 0x82, 0x05, 0x2f,
  0x50, 0x30, 0x21, 0x2e, 0x21, 0xf2, 0x00, 0x2e, 0x00, 0x2e, 0xd0, 0x2e, 0xf0, 0x6f, 0x02, 0x30, 0x02, 0x42, 0x20,
  0x26, 0xe0, 0x6f, 0x02, 0x31, 0x03, 0x40, 0x9a, 0x0a, 0x02, 0x42, 0xf0, 0x37, 0x05, 0x2e, 0x5e, 0xf7, 0x10, 0x08,
  0x12, 0x24, 0x1e, 0xf2, 0x80, 0x42, 0x83, 0x84, 0xf1, 0x7f, 0x0a, 0x25, 0x13, 0x30, 0x83, 0x42, 0x3b, 0x82, 0xf0,
  0x6f, 0x00, 0x2e, 0x00, 0x2e, 0xd0, 0x2e, 0x12, 0x40, 0x52, 0x42, 0x00, 0x2e, 0x12, 0x40, 0x52, 0x42, 0x3e, 0x84,
  0x00, 0x40, 0x40, 0x42, 0x7e, 0x82, 0xe1, 0x7f, 0xf2, 0x7f, 0x98, 0x2e, 0x6a, 0xd6, 0x21, 0x30, 0x23, 0x2e, 0x61,
  0xf5, 0xeb, 0x2c, 0xe1, 0x6f
};

static inline int16_t satneg16(int16_t x)
{
  if (x == INT16_MIN) return INT16_MAX;
  return -x;
}

static inline void bmi270_read(int16_t mag_out[3], int16_t acc_out[3], int16_t gyr_out[3])
{
  uint8_t data[24];
  bmi270_read_burst(0x04, data, 23);
  // for (int i = 0; i < 23; i++) swv_printf("%02x%c", (int)data[i], i == 22 ? '\n' : ' ');
  // Assumes little endian
  mag_out[0] =         (((int16_t)((uint16_t)data[0] << 8) | data[1]) + 0x8000);
  mag_out[1] = satneg16(((int16_t)((uint16_t)data[2] << 8) | data[3]) + 0x8000);
  mag_out[2] = satneg16(((int16_t)((uint16_t)data[4] << 8) | data[5]) + 0x8000);
  acc_out[1] =         (*( int16_t *)(data +  8));
  acc_out[0] =         (*( int16_t *)(data + 10));
  acc_out[2] = satneg16(*( int16_t *)(data + 12));
  gyr_out[1] =         (*( int16_t *)(data + 14));
  gyr_out[0] =         (*( int16_t *)(data + 16));
  gyr_out[2] = satneg16(*( int16_t *)(data + 18));
  data[23] = 0;
  uint32_t time = *(uint32_t *)(data + 20);
  // Gyroscope calibration
  int8_t gyr_cas = ((int8_t)bmi270_read_reg(0x3C) << 1) >> 1;
  gyr_out[0] -= ((uint32_t)gyr_cas * gyr_out[2]) >> 9;

if (0)
  swv_printf("M %6d %6d %6d  A %6d %6d %6d  G %6d %6d %6d\n",
    mag_out[0], mag_out[1], mag_out[2],
    acc_out[0], acc_out[1], acc_out[2],
    gyr_out[0], gyr_out[1], gyr_out[2]);
}

int main()
{
  HAL_Init();

  // ======== GPIO ========
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  GPIO_InitTypeDef gpio_init;

  // SWD (PA13, PA14)
  gpio_init.Pin = GPIO_PIN_13 | GPIO_PIN_14;
  gpio_init.Mode = GPIO_MODE_AF_PP; // Pass over control to AF peripheral
  gpio_init.Alternate = GPIO_AF0_SWJ;
  gpio_init.Pull = GPIO_PULLUP;
  gpio_init.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpio_init);

  // Clocks
  setup_clocks();
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
  swv_printf("sys clock = %u\n", HAL_RCC_GetSysClockFreq());

  // PWR_LATCH
  HAL_GPIO_Init(GPIOC, &(GPIO_InitTypeDef){
    .Pin = GPIO_PIN_14,
    .Mode = GPIO_MODE_OUTPUT_PP,
  });
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, 1);

  // Buttons
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Pin = GPIO_PIN_6,
    .Mode = GPIO_MODE_INPUT,
  });
  HAL_GPIO_Init(GPIOC, &(GPIO_InitTypeDef){
    .Pin = GPIO_PIN_13,
    .Mode = GPIO_MODE_INPUT,
  });

  // LSH_OE (PA4), LED_CLK (PA7), LED_DATA (PA5)
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7,
    .Mode = GPIO_MODE_OUTPUT_PP,
  });
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, 0);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, 1);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 0);
  // Clear LED memory (possibly retained due to close successive power cycles
  // insufficient for the boosted 5 V voltage to decay)
  led_flush();

  uint32_t led_data[24] = {
    0xe1ff0000, 0xe1ff0000, 0xe1ff0000, 0xe1ff0000,
    0xe100ff00, 0xe10000ff, 0xe1c0c000, 0xe100c0c0,
    0xe100ff00, 0xe10000ff, 0xe1c0c000, 0xe100c0c0,
    0xe100ff00, 0xe10000ff, 0xe1c0c000, 0xe100c0c0,
    0xe100ff00, 0xe10000ff, 0xe1c0c000, 0xe100c0c0,
    0xe100ff00, 0xe10000ff, 0xe1c0c000, 0xe100c0c0,
  };
  led_write(led_data, 24);

  // I2Cx
  const uint32_t i2cx_gpioa_pins =
    GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_10;
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Pin = i2cx_gpioa_pins,
    .Mode = GPIO_MODE_OUTPUT_OD,
    .Pull = GPIO_PULLUP,
  });
  GPIOA->BSRR = i2cx_gpioa_pins;  // Set to release signal lines

  const uint32_t i2cx_gpiob_pins =
    GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
    GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14;
  HAL_GPIO_Init(GPIOB, &(GPIO_InitTypeDef){
    .Pin = i2cx_gpiob_pins,
    .Mode = GPIO_MODE_OUTPUT_OD,
    .Pull = GPIO_PULLUP,
  });
  GPIOB->BSRR = i2cx_gpiob_pins;

  read_SDA = _read_SDA_04;
  write_SDA = _write_SDA_04;
  i2c_init();

  while (1) {
    uint8_t chip_id = bmi270_read_reg(0x00);
    swv_printf("BMI270 chip ID = 0x%02x, I2C err = %u, line = %d\n",
      (int)chip_id, i2c_err, i2c_first_err_line);
    if (chip_id != 0x24) {
      HAL_Delay(100);
      continue;
    }

    bmi270_write_reg(0x7E, 0xB6); // Soft reset
    HAL_Delay(1);

    bmi270_write_reg(0x7C, 0x00);
    HAL_Delay(1);
    bmi270_write_reg(0x59, 0x00);
    bmi270_write_burst(bmi270_config_file, sizeof bmi270_config_file);
    bmi270_write_reg(0x59, 0x01);
    HAL_Delay(150);

    // INTERNAL_STATUS
    uint8_t init_status = bmi270_read_reg(0x21) & 0xF;
    if (init_status == 0x1) {
      swv_printf("BMI270 init ok\n");
      break;
    } else {
      swv_printf("BMI270 init status 0x%02x\n", (int)init_status);
    }
  }

  // Normal mode
  bmi270_write_reg(0x7D, 0b0110); // PWR_CTRL.gyr_en = 1, .acc_en = 1
  bmi270_write_reg(0x40, 0xA8);   // ACC_CONF
  bmi270_write_reg(0x41, 0x01);   // ACC_RANGE: +/-4g
  bmi270_write_reg(0x42, 0xA9);   // GYR_CONF
  bmi270_write_reg(0x43, 0x00);   // GYR_RANGE: +/-2000dps
  HAL_Delay(10);

  bmi270_write_reg(0x6B, 0x21);   // IF_CONF.aux_en = 1
  bmi270_write_reg(0x7D, 0b0110); // PWR_CTRL.aux_en = 0
  bmi270_write_reg(0x4B, 0x30 << 1);  // AUX_DEV_ID
  bmi270_write_reg(0x4C, 0b10001111);
    // AUX_IF_CONF
    //   .aux_rd_burst = 0x3 (length 8)
    //   .man_rd_burst = 0x3 (length 8)
    //   .aux_manual_en = 1
  // Write to ODR (0x1A), value 100
  bmi270_write_reg(0x4F, 100);
  bmi270_write_reg(0x4E, 0x1A);
  HAL_Delay(1);
  // Write to Internal Control 0 (0x1B), value 0xA1 (Cmm_freq_en, Auto_SR_en, Take_meas_M)
  bmi270_write_reg(0x4F, 0xA1);
  bmi270_write_reg(0x4E, 0x1B);
  HAL_Delay(1);
  // Write to Internal Control 2 (0x1D), value 0x10 (Cmm_en)
  bmi270_write_reg(0x4F, 0x10);
  bmi270_write_reg(0x4E, 0x1D);
  HAL_Delay(1);

  bmi270_write_reg(0x68, 0x02);   // AUX_IF_TRIM (10 kΩ)
  bmi270_write_reg(0x7D, 0b0111); // PWR_CTRL.aux_en = 1
  bmi270_write_reg(0x4D, 0x00);   // AUX_RD_ADDR
  bmi270_write_reg(0x4C, 0b00001111); // AUX_IF_CONF.aux_manual_en = 0
  HAL_Delay(10);

  for (int i = 0; i < 24; i++) led_data[i] = 0xe10000ff;
  led_write(led_data, 24);

  // Ambient light sensors

  read_SDA = _read_SDA;
  write_SDA = _write_SDA;
  i2c_init();
  bh1750fvi_readout_start(0b0100011 << 1);
  bh1750fvi_readout_start(0b1011100 << 1);

  // LCD_RSTN (PB6), LCD_BL (PB4), LCD_DC (PB5), LCD_CS (PB9)
  HAL_GPIO_Init(GPIOB, &(GPIO_InitTypeDef){
    .Pin = GPIO_PIN_6 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_9,
    .Mode = GPIO_MODE_OUTPUT_PP,
  });
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, 0);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 0);  // Turn off backlight
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 0);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, 1);

  // ======== DMA for SPI ========
  __HAL_RCC_DMA1_CLK_ENABLE();
  DMA_HandleTypeDef dma_spi2_tx;
  dma_spi2_tx.Instance = DMA1_Channel2;
  dma_spi2_tx.Init.Request = DMA_REQUEST_SPI2_TX;
  dma_spi2_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
  dma_spi2_tx.Init.PeriphInc = DMA_PINC_DISABLE;
  dma_spi2_tx.Init.MemInc = DMA_MINC_ENABLE;
  dma_spi2_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  dma_spi2_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  dma_spi2_tx.Init.Mode = DMA_NORMAL;
  dma_spi2_tx.Init.Priority = DMA_PRIORITY_LOW;
  HAL_DMA_Init(&dma_spi2_tx);

  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 15, 1);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
  HAL_NVIC_SetPriority(SPI2_IRQn, 15, 2);
  HAL_NVIC_EnableIRQ(SPI2_IRQn);

  // ======== SPI ========
  // SPI2_MOSI (PB7), SPI2_SCK (PB8)
  HAL_GPIO_Init(GPIOB, &(GPIO_InitTypeDef){
    .Pin = GPIO_PIN_7 | GPIO_PIN_8,
    .Mode = GPIO_MODE_AF_PP,
    .Alternate = GPIO_AF1_SPI2,
    .Speed = GPIO_SPEED_FREQ_HIGH,
  });
  __HAL_RCC_SPI2_CLK_ENABLE();
  spi2 = (SPI_HandleTypeDef){
    .Instance = SPI2,
    .Init = {
      .Mode = SPI_MODE_MASTER,
      .Direction = SPI_DIRECTION_2LINES,
      .CLKPolarity = SPI_POLARITY_LOW,  // CPOL = 0
      .CLKPhase = SPI_PHASE_1EDGE,      // CPHA = 0
      .NSS = SPI_NSS_SOFT,
      .FirstBit = SPI_FIRSTBIT_MSB,
      .TIMode = SPI_TIMODE_DISABLE,
      .CRCCalculation = SPI_CRCCALCULATION_DISABLE,
      .DataSize = SPI_DATASIZE_8BIT,
      .BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2,
    },
  };
  __HAL_LINKDMA(&spi2, hdmatx, dma_spi2_tx);
  HAL_SPI_Init(&spi2);
  __HAL_SPI_ENABLE(&spi2);

  lcd_init();

  // Fill screen with black
  lcd_fill(0x00);
  HAL_Delay(5);
  lcd_bl(1);  // Turn on backlight

  // ======== DMA for I2S ========
  __HAL_RCC_DMA1_CLK_ENABLE();
  DMA_HandleTypeDef dma_i2s1_tx;
  dma_i2s1_tx.Instance = DMA1_Channel1;
  dma_i2s1_tx.Init.Request = DMA_REQUEST_SPI1_TX;
  dma_i2s1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
  dma_i2s1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
  dma_i2s1_tx.Init.MemInc = DMA_MINC_ENABLE;
  dma_i2s1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
  dma_i2s1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
  dma_i2s1_tx.Init.Mode = DMA_CIRCULAR;
  dma_i2s1_tx.Init.Priority = DMA_PRIORITY_LOW;
  HAL_DMA_Init(&dma_i2s1_tx);

  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 14, 1);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  HAL_NVIC_SetPriority(SPI1_IRQn, 14, 2);
  HAL_NVIC_EnableIRQ(SPI1_IRQn);

  // ======== I2S ========
  gpio_init = (GPIO_InitTypeDef){
    .Mode = GPIO_MODE_AF_PP,
    .Alternate = GPIO_AF0_SPI1,
    .Pull = GPIO_NOPULL,
    .Speed = GPIO_SPEED_FREQ_HIGH,
  };
  gpio_init.Pin = GPIO_PIN_12 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOA, &gpio_init);
  gpio_init.Pin = GPIO_PIN_3;
  HAL_GPIO_Init(GPIOB, &gpio_init);

  __HAL_RCC_SPI1_CLK_ENABLE();
  i2s1 = (I2S_HandleTypeDef){
    .Instance = SPI1,
    .Init = {
      .Mode = I2S_MODE_MASTER_TX,
      .Standard = I2S_STANDARD_PHILIPS,
      .DataFormat = I2S_DATAFORMAT_16B,
      .MCLKOutput = I2S_MCLKOUTPUT_DISABLE,
      .AudioFreq = 40000,
      .CPOL = I2S_CPOL_LOW,
    },
  };
  __HAL_LINKDMA(&i2s1, hdmatx, dma_i2s1_tx);
  HAL_I2S_Init(&i2s1);

  swv_printf("sys clock = %u\n", HAL_RCC_GetSysClockFreq());
  swv_printf("I2S clock = %u\n", HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_I2S1));
  // 64000000, divider is 64000000 / 16 / 40k = 100

  // `audio_pcm_buf` is zero-filled (in section `.bss`)
  HAL_I2S_Transmit_DMA(&i2s1, (uint16_t *)audio_pcm_buf, N_AUDIO_PCM_BUF * 2);

  float mag_psi[10][10] = {{ 0 }};
  float m_tfm[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  vec3 m_cen = (vec3){0, 0, 0};

  screen_element el_outer_ring = {
    .update = outer_ring_update,
    .fill_tile = outer_ring_fill_tile,
  };
  screen_elements[n_screen_elements++] = &el_outer_ring;

  screen_element el_compass = {
    .update = compass_update,
    .fill_tile = compass_fill_tile,
  };
  screen_elements[n_screen_elements++] = &el_compass;

  screen_element_text t = text_create();
  t.x = 50;
  t.y = 75;
  t.s = "test\ntext write 1234567890";
  screen_elements[n_screen_elements++] = &t;

  while (1) {
    static int count = 0;

    if (HAL_GetTick() >= 2000 && count % 50 == 0) {
      read_SDA = _read_SDA_06;
      write_SDA = _write_SDA_06;
      i2c_init();

      uint32_t vcell = max17049_read_reg(0x02);
      uint32_t soc = max17049_read_reg(0x04);
      swv_printf("VCELL = %u, SOC = %u %u\n", vcell, soc / 256, soc % 256);
    }

    count++;

    read_SDA = _read_SDA_04;
    write_SDA = _write_SDA_04;

    int16_t mag_out[3], acc_out[3], gyr_out[3];
    bmi270_read(mag_out, acc_out, gyr_out);

    vec3 mag = (vec3){(float)mag_out[0], (float)mag_out[1], (float)mag_out[2]};
    float mag_scale = 2.f / 1024; // unit = 0.5 G = 0.05 mT ≈ geomagnetic field
    mag = vec3_scale(mag, mag_scale);
    elli_fit_insert(mag_psi, mag, 0.01);
    if (count % 25 == 0 && count <= 200) {
      float c[3];
      elli_fit_psi(mag_psi, m_tfm, c);
      m_cen = (vec3){c[0], c[1], c[2]};
      // vec3 m1 = vec3_transform(m_tfm, vec3_diff(mag, m_cen));
      // swv_printf("%6d %6d %6d\n", (int)(m1.x * 1000), (int)(m1.y * 1000), (int)(m1.z * 1000));
      swv_printf("%6d %6d %6d\n", (int)(m_cen.x * 1000), (int)(m_cen.y * 1000), (int)(m_cen.z * 1000));
    }
    // mag = vec3_transform(m_tfm, vec3_diff(mag, m_cen));
    mag = vec3_diff(mag, m_cen);

    float mag_ampl = sqrtf(norm(mag.x, mag.y));
    compass_x = (uint32_t)(( mag.x / mag_ampl * 100 + 120) * 256);
    compass_y = (uint32_t)((-mag.y / mag_ampl * 100 + 120) * 256);

    // Lux meters

    static uint16_t lx[24] = { 0 };
    static uint16_t highest_lx[3] = { 0 };
    if (1) {
      read_SDA = _read_SDA;
      write_SDA = _write_SDA;
      bh1750fvi_readout(0b0100011 << 1, lx);
      bh1750fvi_readout(0b1011100 << 1, lx + 12);
      // Sort
      for (int i = 0; i < 3; i++) highest_lx[i] = 0;
      for (int i = 0, j; i < 24; i++) {
        for (j = 3; j > 0; j--)
          if (lx[i] <= highest_lx[j - 1]) break;
        if (j < 3) {
          for (int k = 2; k > j; k--) highest_lx[k] = highest_lx[k - 1];
          highest_lx[j] = lx[i];
        }
      }
      // TODO: Try Otsu's method?
    }

    max_lx = highest_lx[2];
    uint32_t th1 = (uint32_t)highest_lx[2] / 32;
    uint32_t th2 = (uint32_t)highest_lx[2] / 8;
    uint32_t th3 = (uint32_t)highest_lx[2] / 2;
    if (th1 <= 4) th1 = 4;
    if (th2 <= th1 * 2) th2 = th1 * 2;
    if (th3 <= th2 * 2) th3 = th2 * 2;
    for (int i = 0; i < 24; i++) {
      int index = i / 2 + (i % 2) * 12;
      lx_levels[i] = (
        lx[index] >= th3 ? 3 :
        lx[index] >= th2 ? 2 :
        lx[index] >= th1 ? 1 : 0);
    }

    // Output to LEDs
    uint32_t led_data[24];
    for (int i = 0; i < 24; i++) {
      int index = (17 - i + 24) % 24;
      index = index / 2 + (index % 2) * 12;
      uint16_t value = lx[index];
      led_data[i] = (value >= th2 ? 0xe1000030 :
        value >= th1 ? 0xe1000410 : 0xe1080000);
    }
    led_write(led_data, 24);

    static uint32_t last_tick = 0;
    static uint32_t tile_cnt = TILE_N * TILE_N;

    if (tile_cnt == TILE_N * TILE_N) {
      lcd_new_frame();
      tile_cnt = 0;
      // lcd_brightness(last_screen_refresh % 2 == 0 ? 0x0 : 0xff);
    }

    uint32_t cur_tick = HAL_GetTick();
    if (cur_tick - last_tick >= 50) last_tick = cur_tick;
    else {
      last_tick += 50;  // Wait until
      while (last_tick - HAL_GetTick() < 0x80000000)
        if (tile_cnt < TILE_N * TILE_N) {
          lcd_next_tile(tile_cnt);
          tile_cnt++;
        } else {
          HAL_Delay(1);
        }
    }

    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == 1 || HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == 1) {
      led_flush();
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, 0); // PWR_LATCH
    }
  }
}

void SysTick_Handler()
{
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();
}

static inline void refill_audio(stereo_sample_t *restrict a, int n)
{
  return;
  // 1 s of noise followed by 1 s of triangle wave
  static uint32_t count = 0;
  static uint32_t seed = 20240902;
  for (int i = 0; i < n; i++) {
    if (count < 40000 * 2) {
      if (count < 40000) {
        seed = (seed * 1103515245 + 12345) & 0x7fffffff;
        a[i] = sample((seed >> 5) & 1023);
      } else {
        int32_t phase = (count - 40000) % 64; // 625 Hz
        const int16_t A = 1024;
        // #define abs(_x) ((_x) >= 0 ? (_x) : -(_x))
        inline int16_t abs(int16_t x) { return x >= 0 ? x : -x; }
        int16_t x = abs(A * 2 - (phase - 16) * A * 4 / 64) - A;
        a[i] = sample(x);
      }
      count++;
    } else {
      a[i] = sample(0);
    }
  }
}

void DMA1_Channel1_IRQHandler()
{
  HAL_DMA_IRQHandler(i2s1.hdmatx);
}
void SPI1_IRQHandler()
{
  HAL_I2S_IRQHandler(&i2s1);
}
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef *i2s1)
{
  refill_audio(audio_pcm_buf, N_AUDIO_PCM_BUF / 2);
}
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef *i2s1)
{
  refill_audio(audio_pcm_buf + N_AUDIO_PCM_BUF / 2, N_AUDIO_PCM_BUF / 2);
}

void DMA1_Channel2_3_IRQHandler()
{
  HAL_DMA_IRQHandler(spi2.hdmatx);
}
void SPI2_IRQHandler()
{
  HAL_SPI_IRQHandler(&spi2);
}
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *spi2)
{
  lcd_cs(1);
  // swv_printf("tx ok!\n");
  lcd_dma_busy = false;
}

void NMI_Handler() { while (1) { } }
void HardFault_Handler() { while (1) { } }
void SVC_Handler() { while (1) { } }
void PendSV_Handler() { while (1) { } }
void WWDG_IRQHandler() { while (1) { } }
void RTC_TAMP_IRQHandler() { while (1) { } }
void FLASH_IRQHandler() { while (1) { } }
void RCC_IRQHandler() { while (1) { } }
void EXTI0_1_IRQHandler() { while (1) { } }
void EXTI2_3_IRQHandler() { while (1) { } }
void EXTI4_15_IRQHandler() { while (1) { } }
void DMA1_Ch4_5_DMAMUX1_OVR_IRQHandler() { while (1) { } }
void ADC1_IRQHandler() { while (1) { } }
void TIM1_BRK_UP_TRG_COM_IRQHandler() { while (1) { } }
void TIM1_CC_IRQHandler() { while (1) { } }
void TIM3_IRQHandler() { while (1) { } }
void TIM14_IRQHandler() { while (1) { } }
void TIM16_IRQHandler() { while (1) { } }
void TIM17_IRQHandler() { while (1) { } }
void I2C1_IRQHandler() { while (1) { } }
void I2C2_IRQHandler() { while (1) { } }
void USART1_IRQHandler() { while (1) { } }
void USART2_IRQHandler() { while (1) { } }
