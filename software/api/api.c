// _____________________________________________________________________________
// |                                                                           |
// |  DMC-1 GPU API                                                            |
// |  =============                                                            |
// |                                                                           |
// | @sylefeb             licence: MIT, see full text in repo                  |
// |___________________________________________________________________________|

#include "memmap.h"

// -----------------------------------------------------
// Column API
// -----------------------------------------------------

#define COLDRAW_COL(idx,start,end,light) ((idx) | ((start&255)<<10) | ((end/*>0*/)<<18) | ((light&15)<<26))

#define WALL        (0<<30)
#define PLANE       (1<<30)
#define TERRAIN     (2<<30)
#define PARAMETER   (3<<30)

#define COLDRAW_WALL(y,v_init,u_init) ((  y)/* >0 */) | ((((v_init))&255)<<16) | (((u_init)&255) << 24)
#define COLDRAW_TERRAIN(st,ed,pick)   (( ed)/* >0 */) | ((st/* > 0*/    )<<16) | (pick)
#define COLDRAW_PLANE_B(ded,dr)       ((ded) & 65535) | (((dr) & 65535) << 16)

#define PICK        (1<<31)
#define COLDRAW_EOC (PARAMETER | 1)

// parameter: ray cs (terrain)
#define PARAMETER_RAY_CS(cs,ss)      (0<<30) | (( cs) & 16383) | ((   ss & 16383 )<<14)
// parameter: uv offset
#define PARAMETER_UV_OFFSET(vo)      (1<<30) | ((vo) & 16777215)
#define PARAMETER_UV_OFFSET_EX(uo) (((uo) & 16777215)<<1)
// parameter: plane
#define PARAMETER_PLANE_A(ny,uy,vy)  (2<<30) | ((ny) & 1023)  | (((uy) & 1023)<<10) | (((vy) & 1023)<<20)
#define PARAMETER_PLANE_A_EX(du,dv)  (((du) & 16383)<<1) | (((dv) & 16383)<<15)
// parameter: terrain view height
#define PARAMETER_VIEW_Z(view_z)     (3<<30) | (view_z&65535)

#define Y_MAX        65535

// -----------------------------------------------------

static inline void col_send(unsigned int tex0,unsigned int tex1)
{
  *COLDRAW0 = tex0;
  *COLDRAW1 = tex1;
}

// -----------------------------------------------------

static inline int userdata()
{
  int ud;
  asm volatile ("rdtime %0" : "=r"(ud));
  return ud;
}

// -----------------------------------------------------

static inline int uart_byte()
{
  int id;
  asm volatile ("rdtime %0" : "=r"(id));
  return (id >> 8)&255;
}

// -----------------------------------------------------

static inline int col_full()
{
  return ((userdata()&1) == 0);
}

static inline void col_process()
{
  while (col_full()) { /*wait fifo not full*/ }
}

static inline void wait_all_drawn()
{
	while ((userdata()&4) == 0) { /*wait fifo empty*/ }
}

// -----------------------------------------------------
// Basic CPU functions
// -----------------------------------------------------

static inline unsigned int time()
{
   int cycles;
   asm volatile ("rdcycle %0" : "=r"(cycles));
   return cycles;
}

static inline unsigned int core_id()
{
   unsigned int id;
   asm volatile ("rdcycle %0" : "=r"(id));
   return id&1;
}

static inline void pause(int cycles)
{
  unsigned int tm_start = time();
  while (time() - tm_start < cycles) { }
}

static inline unsigned int btn_left()
{
  return userdata() & (1<<5);
}

static inline unsigned int btn_fwrd()
{
  return userdata() & (1<<6);
}

static inline unsigned int btn_right()
{
  return userdata() & (1<<7);
}

// -----------------------------------------------------
// Peripherals
// -----------------------------------------------------

#include "oled.h"
#include "spiflash.h"

// -----------------------------------------------------
// UART printf
// -----------------------------------------------------

void putchar(int c)
{
  *UART = c;
  pause(10000);
}

static inline void print_string(const char* s)
{
   for (const char* p = s; *p; ++p) {
      putchar(*p);
   }
}

static inline void print_dec(int val)
{
   char buffer[255];
   char *p = buffer;
   if (val < 0) {
      putchar('-');
      print_dec(-val);
      return;
   }
   while (val || p == buffer) {
      int q = val / 10;
      *(p++) = val - q * 10;
      val    = q;
   }
   while (p != buffer) {
      putchar('0' + *(--p));
   }
}

static inline void print_hex_digits(unsigned int val, int nbdigits)
{
   for (int i = (4*nbdigits)-4; i >= 0; i -= 4) {
      putchar("0123456789ABCDEF"[(val >> i) & 15]);
   }
}

static inline void print_hex(unsigned int val)
{
   print_hex_digits(val, 8);
}

#include <stdarg.h>

static inline int printf(const char *fmt,...)
{
  va_list ap;
  for (va_start(ap, fmt);*fmt;fmt++) {
    if (*fmt=='%') {
      fmt++;
      if (*fmt=='s')      print_string(va_arg(ap,char *));
      else if (*fmt=='x') print_hex(va_arg(ap,int));
      else if (*fmt=='d') print_dec(va_arg(ap,int));
      else if (*fmt=='c') putchar(va_arg(ap,int));
      else                putchar(*fmt);
    } else {
      putchar(*fmt);
    }
  }
  va_end(ap);
}

// -----------------------------------------------------
// Arithmetic
// -----------------------------------------------------

static inline int mul(int a, int b)
{
  return a*b;
}

static inline int div(int n, int d)
{
  return n/d;
}

static inline int dot(int a, int b,int c, int d) // return a*b + c*d
{
  return a*b + c*d;
}
