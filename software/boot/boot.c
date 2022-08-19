// _____________________________________________________________________________
// |                                                                           |
// |  Boot loader, resides in BRAM, loads code from ROM to RAM                 |
// |  =========================================================                |
// |                                                                           |
// | Draws a Comanche style terrain using the DMC-1 GPU                        |
// |                                                                           |
// | @sylefeb 2021-05-05  licence: GPL v3, see full text in repo               |
// |___________________________________________________________________________|

volatile int*           const LEDS         = (int          *)0x40020;
volatile unsigned int*  const SPIFLASH     = (unsigned int* )0x40028;

#include "spiflash.h"

#define PAYLOAD_OFFSET (1<<20) // 1MB offset

static inline int core_id()
{
   unsigned int cycles;
   asm volatile ("rdcycle %0" : "=r"(cycles));
   return cycles&1;
}

volatile int synch = 0;

void main_load_code()
{
  *LEDS = 1;
  spiflash_init();
  *LEDS = 2;
  // copy to the start of the memory segment
  unsigned char *code = (unsigned char *)0x0000004;
  spiflash_copy(PAYLOAD_OFFSET/*offset*/,code,131064/*SPRAM size*/);
  //                                          ^^^^^^ max code image size
  // jump!
  *LEDS = 15;
  synch =  1; // sync with other core
  asm volatile ("li t0,4; jalr x0,0(t0);");
}

void main_wait_n_jump()
{
  while (synch == 0) { }
  asm volatile ("li t0,4; jalr x0,0(t0);");
}

void main()
{

  if (core_id() == 0) {

    main_load_code();

  } else {

		main_wait_n_jump();

	}
}
