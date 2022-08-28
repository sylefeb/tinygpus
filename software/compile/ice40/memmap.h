// -----------------------------------------------------
// Memory mapping addresses
// -----------------------------------------------------

volatile unsigned int*  const OLED         = (int          *)0x40024;
volatile unsigned int*  const OLED_RST     = (int          *)0x4002C;
volatile int*           const UART         = (int          *)0x40040;
volatile int*           const LEDS         = (int          *)0x40020;
volatile unsigned int*  const SPIFLASH     = (unsigned int *)0x40028;
volatile unsigned int*  const COLDRAW0     = (unsigned int *)0x40014;
volatile unsigned int*  const COLDRAW1     = (unsigned int *)0x40010;


volatile unsigned int*  const PARAMETER_PLANE_A_ny  = (unsigned int *)0x40080;
volatile unsigned int*  const PARAMETER_PLANE_A_uy  = (unsigned int *)0x40180;
volatile unsigned int*  const PARAMETER_PLANE_A_vy  = (unsigned int *)0x40280;
volatile unsigned int*  const PARAMETER_PLANE_A_EX_du  = (unsigned int *)0x40380;
volatile unsigned int*  const PARAMETER_PLANE_A_EX_dv  = (unsigned int *)0x40480;
volatile unsigned int*  const PARAMETER_UV_OFFSET_v = (unsigned int *)0x40580;
volatile unsigned int*  const PARAMETER_UV_OFFSET_EX_u    = (unsigned int *)0x40680;
volatile unsigned int*  const PARAMETER_UV_OFFSET_EX_lmap = (unsigned int *)0x40780;
volatile unsigned int*  const COLDRAW_PLANE_B_ded = (unsigned int *)0x40880;
volatile unsigned int*  const COLDRAW_PLANE_B_dr  = (unsigned int *)0x40980;
volatile unsigned int*  const COLDRAW_COL_texid   = (unsigned int *)0x40A80;
volatile unsigned int*  const COLDRAW_COL_start   = (unsigned int *)0x40B80;
volatile unsigned int*  const COLDRAW_COL_end     = (unsigned int *)0x40C80;
volatile unsigned int*  const COLDRAW_COL_light   = (unsigned int *)0x40D80;
