#include <stm32g0xx_hal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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
  for (int i = 0; i < n; i++)
    #pragma GCC unroll 32
    for (int j = 0; j < 32; j++) {
      uint8_t b = (data[i] >> (31 - j)) & 1;
      GPIOA->BSRR = (1 << (7 + 16)) |
        (b ? (1 << 5) : (1 << (5 + 16)));
      asm volatile ("nop");
      GPIOA->BSRR = 1 << 7;
    }
  __enable_irq();
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 0);
}
#pragma GCC pop_options

static inline void led_flush()
{
  uint32_t data[26];
  data[0] = 0x0;
  for (int i = 1; i < 25; i++) data[i] = 0xe0000000;
  data[25] = 0xffffffff;
  led_write(data, 26);
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

#pragma GCC push_options
#pragma GCC optimize("O3")
static void i2c_delay()
{
  // 10 us = 100 kHz
  // NOTE: Lower frequency might be used with internal weak pull-ups and/or for debugging
  static const uint32_t us = 100;
  for (int i = 0; i < 64 * us / 4; i++) asm volatile ("nop");
}
static bool read_SCL() { return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10); }
static void set_SCL() { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET); }
static void clear_SCL() { HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET); }

static uint32_t (*read_SDA)();
static void (*write_SDA)();

static uint32_t _read_SDA() {
  return
    (((GPIOA->IDR >> 0) & 1) <<  5) |
    // (((GPIOA->IDR >> 1) & 1) <<  6) |
    (((GPIOB->IDR >> 2) & 1) << 11) |
    0xfffff7df;
}
static void _write_SDA(uint32_t value) {
  uint32_t mask_a = (1 << 0) /* | (1 << 1) */;
  GPIOA->ODR = (GPIOA->ODR & ~mask_a)
    | (((value >>  5) & 1) << 0)
    // | (((value >>  6) & 1) << 1)
    ;
  uint32_t mask_b = 1 << 2;
  GPIOB->ODR = (GPIOB->ODR & ~mask_b) | (((value >> 11) & 1) << 2);
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
  bool bit = (read_SDA() == 0xffffffff);
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

static inline void bh1750fvi_readout(uint8_t addr, uint16_t results[12])
{
  // One Time H-Resolution Mode
  uint8_t op = 0x20;
  i2c_write(addr, &op, 1);
  HAL_Delay(200);
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

  uint32_t led_data[5] = {0x0, 0xe1ff0000, 0xe1ff0000, 0xe1ff0000, 0xffffffff};
  led_write(led_data, 5);

  // I2Cx
  const uint32_t i2cx_gpioa_pins =
    GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_10;
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Pin = i2cx_gpioa_pins,
    .Mode = GPIO_MODE_OUTPUT_OD,
    .Pull = GPIO_PULLUP,
  });
  GPIOA->BSRR = i2cx_gpioa_pins;  // Set to release signal lines

  while (0) {
    GPIOA->ODR ^= (1 << 1);
    uint32_t read_scl = GPIOA->IDR & (1 << 1);
    uint32_t read_sda = GPIOA->IDR & (1 << 10);
    i2c_delay();
    led_data[1] = (GPIOA->ODR & (1 << 1)) ? 0xe100c0c0 : 0xe1ff0000;
    led_data[2] = read_scl ? 0xe100c0c0 : 0xe1ff0000;
    led_data[3] = read_sda ? 0xe100c0c0 : 0xe1ff0000;
    led_write(led_data, 5);
    HAL_Delay(1000);
    swv_printf("%d %08x\n", GPIOA->ODR & (1 << 1), GPIOA->IDR);
    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == 1 || HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == 1) {
      led_flush();
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, 0); // PWR_LATCH
    }
  }

  const uint32_t i2cx_gpiob_pins =
    GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
    GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14;
  HAL_GPIO_Init(GPIOB, &(GPIO_InitTypeDef){
    .Pin = i2cx_gpiob_pins,
    .Mode = GPIO_MODE_OUTPUT_OD,
    .Pull = GPIO_PULLUP,
  });
  GPIOB->BSRR = i2cx_gpiob_pins;

  read_SDA = _read_SDA_06;
  write_SDA = _write_SDA_06;
  i2c_init();

  HAL_Delay(1200);
  uint32_t vcell = max17049_read_reg(0x02);
  uint32_t soc = max17049_read_reg(0x04);
  swv_printf("VCELL = %u, SOC = %u %u\n", vcell, soc / 256, soc % 256);

  read_SDA = _read_SDA_04;
  write_SDA = _write_SDA_04;
  i2c_init();

  uint8_t chip_id = bmi270_read_reg(0x00);
  swv_printf("BMI270 chip ID = 0x%02x, I2C err = %u, line = %d\n",
    (int)chip_id, i2c_err, i2c_first_err_line); // Should read 0x24
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
  } else {
    swv_printf("BMI270 init status 0x%02x\n", (int)init_status);
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

  uint8_t data[24];
  for (int i = 0; i < 10; i++) {
    bmi270_read_burst(0x04, data, 23);
    for (int i = 0; i < 23; i++) swv_printf("%02x%c", (int)data[i], i == 22 ? '\n' : ' ');
  }
  for (int i = 1; i < 4; i++) led_data[i] = 0xe10000ff;
  led_write(led_data, 5);

  // Ambient light sensors

  read_SDA = _read_SDA;
  write_SDA = _write_SDA;
  i2c_init();

  uint16_t lx[12];
  bh1750fvi_readout(0b0100011 << 1, lx);
  swv_printf("%u lx, I2C err = %u\n", lx[5], i2c_err);

  // LCD_RSTN (PB6), LCD_BL (PB4), LCD_DC (PB5), LCD_CS (PB9)
  HAL_GPIO_Init(GPIOB, &(GPIO_InitTypeDef){
    .Pin = GPIO_PIN_6 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_9,
    .Mode = GPIO_MODE_OUTPUT_PP,
  });
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, 0);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, 0);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, 1);

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
  HAL_SPI_Init(&spi2);
  __HAL_SPI_ENABLE(&spi2);

  lcd_init();

  lcd_addr(105, 105, 134, 134);
  static uint8_t p[30 * 30 * 2];
  for (int i = 0; i < 30 * 30 * 2; i++) p[i] = 0xff;
  lcd_data_bulk(p, 30 * 30 * 2);

  while (1) {
    static int count = 0;
    swv_printf("hello\n");
    // HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, (++count) & 1);

    count++;
    for (int i = 0; i < 30 * 30 * 2; i += 2) {
      p[i + 0] = 0xff;
      p[i + 1] = 0xff - (count << 2);
      if (count == 6) {
        p[i + 0] = 0b00000010;
        p[i + 1] = 0b00000000;
      }
    }
    lcd_addr(105, 105, 134, 134);
    lcd_data_bulk(p, 30 * 30 * 2);

    bh1750fvi_readout(0b0100011 << 1, lx);
    char s[64];
    snprintf(s, sizeof s, "%5u %5u %5u lx\nI2C err = %u\nline = %d", lx[5], lx[6], lx[11], i2c_err, i2c_first_err_line);
    lcd_print_str(s, 70, 50);

    // Output to LEDs
    uint32_t led_data[5] = {0x0, 0xe1ff0000, 0xe100ff00, 0xe10000ff, 0xffffffff};
    led_write(led_data, 5);

    HAL_Delay(100);
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
void DMA1_Channel1_IRQHandler() { while (1) { } }
void DMA1_Channel2_3_IRQHandler() { while (1) { } }
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
void SPI1_IRQHandler() { while (1) { } }
void SPI2_IRQHandler() { while (1) { } }
void USART1_IRQHandler() { while (1) { } }
void USART2_IRQHandler() { while (1) { } }
