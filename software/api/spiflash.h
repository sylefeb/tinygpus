static inline int spiflash_busy()
{
  int id;
  asm volatile ("rdtime %0" : "=r"(id));
  return (id & 2);
}

static inline void spiflash_init()
{
  while (spiflash_busy()) { /*nothing*/ }
}

static inline unsigned char *spiflash_copy(int addr,volatile int *dst,int len)
{
  *SPIFLASH = (1<<31) | len;
  *SPIFLASH = (int)dst;
  *SPIFLASH = (1<<30) | addr;
  while (spiflash_busy()) {  } // wait
}
