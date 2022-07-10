// -----------------------------------------------------
// Memory mapping addresses
// -----------------------------------------------------

volatile unsigned int*  const OLED         = (int*)0x40024;
volatile unsigned int*  const OLED_RST     = (int*)0x4002C;
volatile int*           const UART         = (int          *)0x40040;
volatile int*           const LEDS         = (int          *)0x40020;
volatile unsigned int*  const SPIFLASH     = (unsigned int* )0x40028;
volatile unsigned int*  const COLDRAW0     = (unsigned int* )0x40014;
volatile unsigned int*  const COLDRAW1     = (unsigned int* )0x40010;
