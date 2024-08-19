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
static inline void blast_led(const uint32_t *data, size_t n)
{
  for (int i = 0; i < n; i++)
    #pragma GCC unroll 32
    for (int j = 0; j < 32; j++) {
      uint8_t b = (data[i] >> (31 - j)) & 1;
      GPIOA->BSRR = (1 << (7 + 16)) |
        (b ? (1 << 5) : (1 << (5 + 16)));
      asm volatile ("nop");
      GPIOA->BSRR = 1 << 7;
    }
}
#pragma GCC pop_options

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

  // LCD_BL
  HAL_GPIO_Init(GPIOB, &(GPIO_InitTypeDef){
    .Pin = GPIO_PIN_4,
    .Mode = GPIO_MODE_OUTPUT_PP,
  });
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, 1);

  // LSH_OE (PA4), LED_CLK (PA7), LED_DATA (PA5)
  HAL_GPIO_Init(GPIOA, &(GPIO_InitTypeDef){
    .Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7,
    .Mode = GPIO_MODE_OUTPUT_PP,
  });
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, 1);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, 0);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, 1);

  while (1) {
    static int count = 0;
    swv_printf("hello\n");
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, (++count) & 1);

    // Output to LEDs
    uint32_t led_data[5] = {0x0, 0xe1ff0000, 0xe100ff00, 0xe10000ff, 0xffffffff};
    blast_led(led_data, 5);

    HAL_Delay(count * 100);
    if (count == 6) HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, 0);
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
