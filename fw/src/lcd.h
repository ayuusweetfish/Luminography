#define lcd_rstn(_v) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, _v)
#define lcd_bl(_v)   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, _v)
#define lcd_dc(_v)   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, _v)
#define lcd_cs(_v)   HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, _v)

static inline void lcd_data(uint8_t x)
{
  lcd_cs(0);
  HAL_SPI_Transmit(&spi2, &x, 1, 1000);
/*
  while (!(SPI2->SR & SPI_SR_TXE)) { }
  SPI2->DR = x;
  while (!(SPI2->SR & SPI_SR_TXE)) { }
  while ((SPI2->SR & SPI_SR_BSY)) { }
  // Clear OVR flag
  (void)SPI2->DR;
  (void)SPI2->SR;
*/
  lcd_cs(1);
}

static inline void lcd_data16(uint16_t x)
{
  lcd_data(x >> 8);
  lcd_data(x & 0xff);
}

static inline void lcd_data_bulk(const uint8_t *x, uint32_t n)
{
  lcd_cs(0);
  HAL_SPI_Transmit(&spi2, (uint8_t *)x, n, 1000);
  lcd_cs(1);
}

static inline void lcd_data_bulk_dma(const uint8_t *x, uint32_t n)
{
  lcd_cs(0);
  HAL_SPI_Transmit_DMA(&spi2, (uint8_t *)x, n);
}

static inline void lcd_reg(uint8_t x)
{
  lcd_dc(0);
  lcd_data(x);
  lcd_dc(1);
}

static inline void lcd_init()
{
  // Adapted from reference firmware source from the manufacturer

  lcd_rstn(0);
  HAL_Delay(100);
  lcd_rstn(1);
  HAL_Delay(100);
  lcd_bl(1);
  HAL_Delay(100);

  lcd_reg(0xEF);

  lcd_reg(0xEB);
  lcd_data(0x14);

  lcd_reg(0xFE);
  lcd_reg(0xEF);

  lcd_reg(0xEB);
  lcd_data(0x14);

  lcd_reg(0x84);
  lcd_data(0x40);

  lcd_reg(0x85);
  lcd_data(0xFF);

  lcd_reg(0x86);
  lcd_data(0xFF);

  lcd_reg(0x87);
  lcd_data(0xFF);

  lcd_reg(0x88);
  lcd_data(0x0A);

  lcd_reg(0x89);
  lcd_data(0x21);

  lcd_reg(0x8A);
  lcd_data(0x00);

  lcd_reg(0x8B);
  lcd_data(0x80);

  lcd_reg(0x8C);
  lcd_data(0x01);

  lcd_reg(0x8D);
  lcd_data(0x01);

  lcd_reg(0x8E);
  lcd_data(0xFF);

  lcd_reg(0x8F);
  lcd_data(0xFF);

  lcd_reg(0xB6);
  lcd_data(0x00);
  lcd_data(0x20);

  lcd_reg(0x36);
  lcd_data(0x08);

  lcd_reg(0x3A);
  lcd_data(0x05);

  lcd_reg(0x90);
  lcd_data(0x08);
  lcd_data(0x08);
  lcd_data(0x08);
  lcd_data(0x08);

  lcd_reg(0xBD);
  lcd_data(0x06);

  lcd_reg(0xBC);
  lcd_data(0x00);

  lcd_reg(0xFF);
  lcd_data(0x60);
  lcd_data(0x01);
  lcd_data(0x04);

  lcd_reg(0xC3);
  lcd_data(0x13);
  lcd_reg(0xC4);
  lcd_data(0x13);

  lcd_reg(0xC9);
  lcd_data(0x22);

  lcd_reg(0xBE);
  lcd_data(0x11);

  lcd_reg(0xE1);
  lcd_data(0x10);
  lcd_data(0x0E);

  lcd_reg(0xDF);
  lcd_data(0x21);
  lcd_data(0x0c);
  lcd_data(0x02);

  lcd_reg(0xF0);
  lcd_data(0x45);
  lcd_data(0x09);
  lcd_data(0x08);
  lcd_data(0x08);
  lcd_data(0x26);
  lcd_data(0x2A);

  lcd_reg(0xF1);
  lcd_data(0x43);
  lcd_data(0x70);
  lcd_data(0x72);
  lcd_data(0x36);
  lcd_data(0x37);
  lcd_data(0x6F);

  lcd_reg(0xF2);
  lcd_data(0x45);
  lcd_data(0x09);
  lcd_data(0x08);
  lcd_data(0x08);
  lcd_data(0x26);
  lcd_data(0x2A);

  lcd_reg(0xF3);
  lcd_data(0x43);
  lcd_data(0x70);
  lcd_data(0x72);
  lcd_data(0x36);
  lcd_data(0x37);
  lcd_data(0x6F);

  lcd_reg(0xED);
  lcd_data(0x1B);
  lcd_data(0x0B);

  lcd_reg(0xAE);
  lcd_data(0x77);

  lcd_reg(0xCD);
  lcd_data(0x63);

  lcd_reg(0x70);
  lcd_data(0x07);
  lcd_data(0x07);
  lcd_data(0x04);
  lcd_data(0x0E);
  lcd_data(0x0F);
  lcd_data(0x09);
  lcd_data(0x07);
  lcd_data(0x08);
  lcd_data(0x03);

  lcd_reg(0xE8);
  lcd_data(0x34);

  lcd_reg(0x62);
  lcd_data(0x18);
  lcd_data(0x0D);
  lcd_data(0x71);
  lcd_data(0xED);
  lcd_data(0x70);
  lcd_data(0x70);
  lcd_data(0x18);
  lcd_data(0x0F);
  lcd_data(0x71);
  lcd_data(0xEF);
  lcd_data(0x70);
  lcd_data(0x70);

  lcd_reg(0x63);
  lcd_data(0x18);
  lcd_data(0x11);
  lcd_data(0x71);
  lcd_data(0xF1);
  lcd_data(0x70);
  lcd_data(0x70);
  lcd_data(0x18);
  lcd_data(0x13);
  lcd_data(0x71);
  lcd_data(0xF3);
  lcd_data(0x70);
  lcd_data(0x70);

  lcd_reg(0x64);
  lcd_data(0x28);
  lcd_data(0x29);
  lcd_data(0xF1);
  lcd_data(0x01);
  lcd_data(0xF1);
  lcd_data(0x00);
  lcd_data(0x07);

  lcd_reg(0x66);
  lcd_data(0x3C);
  lcd_data(0x00);
  lcd_data(0xCD);
  lcd_data(0x67);
  lcd_data(0x45);
  lcd_data(0x45);
  lcd_data(0x10);
  lcd_data(0x00);
  lcd_data(0x00);
  lcd_data(0x00);

  lcd_reg(0x67);
  lcd_data(0x00);
  lcd_data(0x3C);
  lcd_data(0x00);
  lcd_data(0x00);
  lcd_data(0x00);
  lcd_data(0x01);
  lcd_data(0x54);
  lcd_data(0x10);
  lcd_data(0x32);
  lcd_data(0x98);

  lcd_reg(0x74);
  lcd_data(0x10);
  lcd_data(0x85);
  lcd_data(0x80);
  lcd_data(0x00);
  lcd_data(0x00);
  lcd_data(0x4E);
  lcd_data(0x00);

  lcd_reg(0x98);
  lcd_data(0x3e);
  lcd_data(0x07);

  lcd_reg(0x35);
  lcd_reg(0x21);

  lcd_reg(0x11);
  HAL_Delay(120);
  lcd_reg(0x29);
  HAL_Delay(20);
}

static inline void lcd_addr(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  lcd_reg(0x2a);
  lcd_data16(x1);
  lcd_data16(x2);
  lcd_reg(0x2b);
  lcd_data16(y1);
  lcd_data16(y2);
  lcd_reg(0x2c);
}
